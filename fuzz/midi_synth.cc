// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// The MIDI that a host sends, and the synthesis it drives. The input stands
// for everything a host and a project can ask of the player: which emulator
// plays, how many chips it has, what the chip settings are, and then a stream
// of MIDI messages and of audio to generate. It is the way into the real-time
// API of libADLMIDI / libOPNMIDI, which the file loaders never touch.
//
// The input is a header, then records. A byte that the input does not reach is
// read as zero, and a zero means "leave it as it is", so an empty input plays
// the default chip settings and makes no sound:
//
//   byte 0  the emulator, one more than its number
//   byte 1  the chips, one more than how many (one or two)
//   byte 2  the settings that are on or off: 4 soft panning, and for OPL3
//           1 deep tremolo, 2 deep vibrato, 8 the MT-32 defaults; for OPN2
//           1 the LFO, and the bits above the fourth its frequency
//   byte 3  the volume model, one more than its number
//   byte 4  OPL3: the four-operator channels, one more than how many
//           OPN2: the chip type, one more than its number
//
// A record is one byte, and the bits at its top say what it is:
//
//   00xxxxxx  a MIDI message of 1 + (x & 3) bytes, which follow
//   010xxxxx  generate (1 + x) * 32 frames of audio
//   011xxxxx  a note on the channel x & 15: the note and the velocity follow,
//             seven bits of each. A message of three bytes says the same, but
//             this way a note is one record and every byte of it is a note:
//             the fuzzer reaches the synthesis instead of stopping at the
//             status bytes, and a seed is readable
//   10xxxxxx  a System Exclusive message of x bytes, which follow; the 0xf0 at
//             the start and the 0xf7 at the end are put on here, the way a host
//             hands one over
//   11xxxxxx  what else a player is asked to do: reset, panic, and the chip
//             settings again, which a project does when it is restored
//
// What is checked: the sanitizers, and that every sample is a finite number. A
// chip that is asked for silence answers with silence, not with a NaN that
// spreads through the mix of a host.
//
// The audio and the events are capped, so that one input is a millisecond or
// two of work for an ordinary core. The low-level cores are a hundred times
// slower than that, and the cap keeps even those at a fraction of a second.

#include "fuzz.h"
#include "adl/player.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <span>
#include <vector>

namespace {

// The audio of one input, in frames, and how many records it may have.
constexpr unsigned frames_max = 4096;
constexpr unsigned frames_at_once = 1024;
constexpr unsigned records_max = 1024;

// The bank of instruments that the notes play, which CMake names. It is read
// once: the file is the same for every input, and the target for bank files is
// what looks at the reading of them.
std::span<const std::uint8_t> bank_data()
{
    static const std::vector<std::uint8_t> bytes = [] {
        std::ifstream file(ADLPLUG_FUZZ_BANK, std::ios::binary);
        std::vector<std::uint8_t> read {std::istreambuf_iterator<char>(file),
                                        std::istreambuf_iterator<char>()};
        FUZZ_CHECK(!read.empty());
        return read;
    }();
    return bytes;
}

// The input, as the records read it. Past the end it is zeroes.
class Input {
public:
    Input(const std::uint8_t *data, std::size_t size) noexcept
        : data_(data), size_(size) {}

    bool done() const noexcept
        { return at_ >= size_; }

    std::uint8_t byte() noexcept
        { return (at_ < size_) ? data_[at_++] : 0; }

    // The next `count` bytes, as many as there are.
    std::vector<std::uint8_t> bytes(std::size_t count)
    {
        const std::size_t have = std::min(count, size_ - std::min(at_, size_));
        std::vector<std::uint8_t> out(data_ + at_, data_ + at_ + have);
        at_ += have;
        return out;
    }

private:
    const std::uint8_t *data_;
    std::size_t size_;
    std::size_t at_ = 0;
};

// A value that the input gives as one more than itself, so that a zero leaves
// the setting alone.
bool given(std::uint8_t byte, unsigned &value) noexcept
{
    if (byte == 0)
        return false;
    value = byte - 1u;
    return true;
}

void apply_settings(Player &pl, Input &input)
{
    // The numbers go in as they come: a saved project holds an emulator and a
    // volume model that the plugin did not write, and both reach the library.
    unsigned value = 0;
    if (given(input.byte(), value))
        pl.set_emulator(value);
    if (given(input.byte(), value))
        pl.set_num_chips(1u + (value & 1u));

    const std::uint8_t flags = input.byte();
    pl.set_soft_pan_enabled((flags & 4u) != 0);
#if defined(ADLPLUG_OPL3)
    pl.set_deep_tremolo((flags & 1u) != 0);
    pl.set_deep_vibrato((flags & 2u) != 0);
    pl.set_mt32_defaults((flags & 8u) != 0);
#elif defined(ADLPLUG_OPN2)
    pl.set_lfo_enabled((flags & 1u) != 0);
    pl.set_lfo_frequency(static_cast<int>((flags >> 4) & 7u));
#endif

    if (given(input.byte(), value))
        pl.set_volume_model(static_cast<int>(value));
#if defined(ADLPLUG_OPL3)
    if (given(input.byte(), value))
        pl.set_num_4ops(value & 0x3fu);
#elif defined(ADLPLUG_OPN2)
    if (given(input.byte(), value))
        pl.set_chip_type(value & 3u);
#endif
}

// Generates `frames` frames, in the pieces the plugin generates them in, and
// looks at every sample.
void generate(Player &pl, unsigned frames)
{
    static std::vector<float> left(frames_at_once), right(frames_at_once);

    while (frames > 0) {
        const unsigned now = std::min(frames, frames_at_once);
        std::fill_n(left.begin(), now, 0.0f);
        std::fill_n(right.begin(), now, 0.0f);
        pl.generate(left.data(), right.data(), now, 1);
        for (unsigned i = 0; i < now; ++i)
            FUZZ_CHECK(std::isfinite(left[i]) && std::isfinite(right[i]));
        frames -= now;
    }
}

void play(Player &pl, Input &input)
{
    unsigned frames_left = frames_max;
    unsigned records_left = records_max;

    while (!input.done() && records_left-- > 0) {
        const std::uint8_t record = input.byte();
        const unsigned value = record & 0x3fu;

        switch (record >> 6) {
        case 0: {
            const std::vector<std::uint8_t> message = input.bytes(1u + (value & 3u));
            if (!message.empty())
                pl.play_midi(message.data(), static_cast<unsigned>(message.size()));
            break;
        }
        case 1:
            if ((record & 0x20u) == 0) {
                const unsigned frames = std::min((1u + (value & 0x1fu)) * 32u, frames_left);
                generate(pl, frames);
                frames_left -= frames;
                if (frames_left == 0)
                    return;
            }
            else {
                const std::uint8_t note = input.byte() & 0x7fu;
                const std::uint8_t velocity = input.byte() & 0x7fu;
                const std::uint8_t message[3] {
                    static_cast<std::uint8_t>(0x90u | (value & 0x0fu)), note, velocity};
                pl.play_midi(message, 3);
            }
            break;
        case 2: {
            std::vector<std::uint8_t> message = input.bytes(value);
            message.insert(message.begin(), 0xf0);
            message.push_back(0xf7);
            pl.play_sysex(message.data(), static_cast<unsigned>(message.size()));
            break;
        }
        default:
            switch (value & 7u) {
            case 0:
                pl.reset();
                break;
            case 1:
                pl.panic();
                break;
            default:
                apply_settings(pl, input);
                break;
            }
            break;
        }
    }
}

}  // namespace

int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    Input input(data, size);

    Player pl;
    pl.init(44100);
    const std::span<const std::uint8_t> bank = bank_data();
    FUZZ_CHECK(pl.load_bank_data(bank.data(), bank.size()));

    apply_settings(pl, input);
    play(pl, input);
    return 0;
}
