// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// What a host does to the plugin while it is loaded: the MIDI it puts in a
// buffer, the automation it writes to the parameters, and the blocks of audio it
// asks for. The state that a project holds arrives whole, and fuzz/state.cc
// reads that; this is the other side of the same border, where the host comes
// back every few milliseconds for as long as the plugin is open.
//
// The input is a header of two bytes, then records. A byte that the input does
// not reach is read as zero, and a zero asks for the ordinary rate and size, so
// an empty input prepares the plugin and asks it for nothing.
//
//   byte 0  the sample rate: one of the rates hosts use, by its place among them,
//           or past those, the four bytes that follow as the bits of the number
//           itself -- any rate a host could pass, including none and no number
//   byte 1  the greatest number of samples in a block: one of the sizes hosts
//           ask for, by its place among them, or past those, the two bytes that
//           follow as the number itself, which may be none or less than none
//
// A record is one byte, and the bits at its top say what it is:
//
//   00xxxxxx  a MIDI message of 1 + (x & 3) bytes, which follow, and then the
//             sample of the next block that it sits at, which the host chooses and
//             need not keep inside the block. Bytes that are not a message are
//             dropped by the buffer, as they are by a host's
//   010xxxxx  a block of 1 + x * 32 samples, or of the whole block if that is
//             fewer. The plugin cuts a block into segments of its own, so the
//             number matters as much as the messages in it
//   011xxxxx  a note on the channel x & 15: the note and the velocity follow,
//             seven bits of each. A message of three bytes says the same, but
//             this way a note is one record and the fuzzer reaches the
//             synthesis instead of stopping at the status bytes
//   10xxxxxx  automation: a byte follows which, with the six bits here, names one
//             of the plugin's parameters, and four bytes after it are the bits of
//             the value it is given -- a host should send a number from nought to
//             one, and these reach what happens when it does not
//   11xxxxxx  what else a host does: prepare the plugin again (which is where it
//             carries its state over; once at most), a bypassed block -- or, with
//             the third bit of x, a block whose player another thread holds, as
//             the worker does while it measures an instrument, and with the fourth
//             bit that block not bypassed -- reset, or with the third bit of x a
//             block of as many channels as the byte after it says, whose channels
//             are nowhere when the highest bit of that byte is set, and all notes
//             off on every channel
//
// What is checked: the sanitizers, that every sample that comes out is a finite
// number, and that the count of the notes sounding on a channel is the number of
// notes the plugin holds to be sounding. The editor shows both, and a panic
// works from the count; if the two parted, the plugin would either show notes
// that are not there or hold notes that nothing can silence.
//
// The audio one input may ask for is capped, as it is in fuzz/midi_synth.cc:
// automation can put a low-level emulator in, and those are a hundred times
// slower than the rest.

#include "fuzz.h"
#include "plugin_processor.h"
#include "JuceHeader.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <semaphore>
#include <thread>
#include <vector>

namespace {

// The audio of one input and how many records it may have. The frames are a
// ceiling; what an input may really ask for is a stretch of time, since the work
// of a block is the audio it covers and not the samples it holds: thirty-three
// samples at a rate of one are thirty-three seconds of sound, which the chips
// play at about the speed of the clock with the sanitizers on top. A tenth of a
// second at the rate the host gave keeps an input to a few milliseconds of work
// whatever rate it asks for, and at a rate too low for a single sample of that,
// the blocks it asks for are none. What those milliseconds cost still follows
// the chips and the emulator core that automation may put in, which this says
// nothing about: libFuzzer is given a minute an input, ten under the memory
// sanitizer (fuzz/CMakeLists.txt), so that a block which costs more than that is
// told of rather than worked on.
constexpr unsigned frames_max = 4096;
constexpr double seconds_max = 0.1;
constexpr unsigned records_max = 512;

// How many times one input may have the plugin prepared again. Each time carries
// the state from the old player to the new one, writing it and reading it back,
// and the default bank alone makes that state large: measured under the address
// sanitizer on a desktop, an input of sixteen of them took 30 seconds on OPN2,
// and nothing else bounded how many an input could have. One is enough for what
// the record is there to reach: the plugin is prepared before the first record,
// so the one is a prepare after a prepare, which carries the state over.
constexpr unsigned prepares_max = 1;

// What hosts ask for. The first of each is what an input that says nothing gets,
// and the last is nothing at all: a host that has not settled its audio, or is
// only looking the plugin over, may prepare it with no rate and no block, and the
// plugin has to come back from that as well.
constexpr double rates[] {44100.0, 22050.0, 32000.0, 48000.0, 88200.0, 96000.0, 192000.0, 0.0};
constexpr int sizes[] {512, 1, 16, 64, 128, 256, 1024, 2048, 0};

// The input, as the records read it. Past the end it is zeroes.
class Input {
public:
    Input(const std::uint8_t *data, std::size_t size) noexcept
        : data_(data), size_(size) {}

    [[nodiscard]] bool done() const noexcept
        { return at_ >= size_; }

    std::uint8_t byte() noexcept
        { return (at_ < size_) ? data_[at_++] : 0; }

    // The next `count` bytes, whatever they are, with zeros past the end of the
    // input as everywhere else.
    std::vector<std::uint8_t> bytes(std::size_t count)
    {
        std::vector<std::uint8_t> out;
        out.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
            out.push_back(byte());
        return out;
    }

private:
    const std::uint8_t *data_;
    std::size_t size_;
    std::size_t at_ = 0;
};

// The count a channel keeps must be the number of notes it holds.
void check_notes(const AdlplugAudioProcessor &processor)
{
    for (unsigned channel = 0; channel < 16; ++channel) {
        unsigned held = 0;
        for (unsigned note = 0; note < 128; ++note)
            held += processor.midi_channel_note_active(channel, note) ? 1u : 0u;
        FUZZ_CHECK(processor.midi_channel_note_count(channel) == held);
    }
}

// One block, of `frames` samples out of the buffers, the way a host gives the
// plugin fewer samples than the most it said it would.
void play_block(AdlplugAudioProcessor &processor, std::vector<float> &left,
                std::vector<float> &right, juce::MidiBuffer &midi, unsigned frames,
                bool bypassed)
{
    std::fill_n(left.begin(), frames, 0.0f);
    std::fill_n(right.begin(), frames, 0.0f);
    // The samples are written through this, by the processor the buffer is handed
    // to. The check looks for a write through the array itself, finds none, and
    // offers a pointee that is const -- which the buffer could not be given
    // either, since it refers to the samples to write them.
    // NOLINTNEXTLINE(misc-const-correctness)
    float *channels[2] {left.data(), right.data()};
    juce::AudioBuffer<float> buffer(channels, 2, static_cast<int>(frames));

    if (bypassed)
        processor.processBlockBypassed(buffer, midi);
    else
        processor.processBlock(buffer, midi);
    midi.clear();

    for (unsigned i = 0; i < frames; ++i)
        FUZZ_CHECK(std::isfinite(left[i]) && std::isfinite(right[i]));
    check_notes(processor);
}

// A block of as many channels as the host gave. The plugin is a stereo one and
// says so, but the buffer is the host's: none at all, or one channel, is what a
// host that has not read the plugin's answer hands over, and the samples of the
// channels it did give must still be numbers.
void play_block_of_channels(AdlplugAudioProcessor &processor, std::vector<float> &left,
                            std::vector<float> &right, juce::MidiBuffer &midi, unsigned frames,
                            unsigned channels, bool nowhere)
{
    // A host with nothing to process may hand over a buffer whose channels are
    // nowhere at all: no samples, and no address to put them at.
    if (nowhere)
        frames = 0;

    // A number no audio reaches, left in every sample the plugin is given: what
    // the plugin plays in, it writes, so a sample that still holds this is one the
    // plugin left alone, and a host that hands over one channel would have been
    // given silence where it asked for sound.
    constexpr float untouched = 12345.0f;
    std::fill_n(left.begin(), frames, untouched);
    std::fill_n(right.begin(), frames, untouched);

    // The samples are written through this, by the processor the buffer is handed
    // to. The check looks for a write through the array itself, finds none, and
    // offers a pointee that is const -- which the buffer could not be given
    // either, since it refers to the samples to write them.
    // NOLINTNEXTLINE(misc-const-correctness)
    float *pointers[2] {nowhere ? nullptr : left.data(), nowhere ? nullptr : right.data()};
    juce::AudioBuffer<float> buffer(pointers, static_cast<int>(channels), static_cast<int>(frames));
    processor.processBlock(buffer, midi);

    for (unsigned i = 0; i < frames; ++i) {
        if (channels > 0) {
            FUZZ_CHECK(left[i] != untouched);
            FUZZ_CHECK(std::isfinite(left[i]));
        }
        if (channels > 1) {
            FUZZ_CHECK(right[i] != untouched);
            FUZZ_CHECK(std::isfinite(right[i]));
        }
    }

    processor.processBlockBypassed(buffer, midi);
    midi.clear();
    check_notes(processor);
}

// A block while another thread holds the player, which is what the worker does
// while it measures an instrument or reconfigures the chips. A host may ask for
// a block at any moment, bypassed or not, and a plugin that could not take the
// player for one has to come back from it all the same.
void play_block_with_the_player_held(AdlplugAudioProcessor &processor, std::vector<float> &left,
                                    std::vector<float> &right, juce::MidiBuffer &midi,
                                    unsigned frames, bool bypassed)
{
    std::binary_semaphore held {0};
    std::binary_semaphore done {0};
    std::thread holder([&processor, &held, &done] {
        const std::unique_lock<std::mutex> lock = processor.acquire_player_nonrt();
        held.release();
        done.acquire();
    });

    held.acquire();
    play_block(processor, left, right, midi, frames, bypassed);
    done.release();
    holder.join();
}

}  // namespace

// The processor's interface is not built here, so this target says what the
// processor has instead of an editor: nothing (plugin_editor.cc holds the other
// two lines of this pair).
bool AdlplugAudioProcessor::hasEditor() const
{
    return false;
}

juce::AudioProcessorEditor *AdlplugAudioProcessor::createEditor()
{
    return nullptr;
}

int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    // The parameters of a processor want JUCE alive. libFuzzer calls this in one
    // thread, so one initialiser for the whole run will do.
    static const juce::ScopedJuceInitialiser_GUI juce_alive;

    Input input(data, size);
    AdlplugAudioProcessor processor;

    // The rate and the size of a block are the host's to choose, and it need not
    // choose one the plugin would have picked. A byte names one of the rates hosts
    // ask for; past those, four bytes are the bits of the number itself, which
    // reaches every rate a host could pass -- an odd one, the largest the type
    // holds, a negative one, and one that is no number at all.
    const unsigned rate_code = input.byte();
    double rate = 0.0;
    if (rate_code < std::size(rates)) {
        rate = rates[rate_code];
    }
    else {
        const std::vector<std::uint8_t> bits = input.bytes(sizeof(float));
        float given = 0.0f;
        std::memcpy(&given, bits.data(), bits.size());
        rate = static_cast<double>(given);
    }
    const unsigned size_code = input.byte();
    int block = 0;
    if (size_code < std::size(sizes)) {
        block = sizes[size_code];
    }
    else {
        const std::vector<std::uint8_t> bits = input.bytes(sizeof(std::int16_t));
        std::int16_t given = 0;
        std::memcpy(&given, bits.data(), bits.size());
        block = given;
    }
    // The plugin is told the number above, whatever it is; the buffers here hold
    // what can be held, and a block asks for no more samples than they have.
    const auto buffered = static_cast<std::size_t>(std::max(0, block));
    processor.prepareToPlay(rate, block);

    // The buffers hold a sample even when the block is none, so that the pointers
    // a buffer is made of are pointers: what the plugin is told is still nothing.
    std::vector<float> left(std::max<std::size_t>(1, buffered));
    std::vector<float> right(std::max<std::size_t>(1, buffered));
    juce::MidiBuffer midi;
    // A rate that is no rate asks for no audio: the plugin keeps a rate of its own
    // for one, and the samples of a tenth of a second of it are none. The test is
    // for a number greater than nought rather than against the ends, since
    // nothing is greater, less or equal to a number that is none.
    const double wanted = std::floor(rate * seconds_max);
    unsigned frames_left = (wanted > 0.0)
        ? static_cast<unsigned>(std::min<double>(wanted, frames_max)) : 0u;
    unsigned records_left = records_max;
    unsigned prepares_left = prepares_max;

    while (!input.done() && records_left-- > 0) {
        const std::uint8_t record = input.byte();
        const unsigned value = record & 0x3fu;

        switch (record >> 6) {
        case 0: {
            const std::vector<std::uint8_t> message = input.bytes(1u + (value & 3u));
            // The sample an event sits at is the host's to choose: a buffer takes
            // any number, and a host need not keep inside the block it asked for.
            // The byte is read as a signed offset on purpose, as above.
            // NOLINTNEXTLINE(bugprone-signed-char-misuse)
            const auto at = static_cast<int>(static_cast<std::int8_t>(input.byte()));
            midi.addEvent(message.data(), static_cast<int>(message.size()), at);
            break;
        }
        case 1:
            if ((record & 0x20u) == 0) {
                const unsigned frames = std::min({static_cast<unsigned>(buffered),
                                                  1u + (value & 0x1fu) * 32u, frames_left});
                if (frames == 0)
                    return 0;
                play_block(processor, left, right, midi, frames, false);
                frames_left -= frames;
            }
            else {
                const std::uint8_t note = input.byte() & 0x7fu;
                const std::uint8_t velocity = input.byte() & 0x7fu;
                const std::uint8_t message[3] {
                    static_cast<std::uint8_t>(0x90u | (value & 0x0fu)), note, velocity};
                midi.addEvent(message, 3, 0);
            }
            break;
        case 2: {
            // Automation, as a host writes it: the value goes in over the whole
            // range of the parameter, and the plugin hears of it as it would
            // from a host or from its own interface.
            const juce::Array<juce::AudioProcessorParameter *> &parameters = processor.getParameters();
            const unsigned which = (value << 8) | input.byte();
            // The bits of the value: a host should send a number from nought to
            // one, and these reach what happens when it sends something else.
            const std::vector<std::uint8_t> bits = input.bytes(sizeof(float));
            float to = 0.0f;
            std::memcpy(&to, bits.data(), bits.size());
            if (!parameters.isEmpty())
                parameters[static_cast<int>(which % static_cast<unsigned>(parameters.size()))]
                    ->setValueNotifyingHost(to);
            break;
        }
        default:
            switch (value & 3u) {
            case 0:
                // A host stops and starts the plugin again, which is where it
                // carries its state from the old player to the new one.
                if (prepares_left > 0) {
                    --prepares_left;
                    processor.releaseResources();
                    processor.prepareToPlay(rate, block);
                }
                break;
            case 1: {
                // A bypassed block, and with one bit more, a block whose player
                // another thread holds.
                const unsigned frames = std::min(static_cast<unsigned>(buffered), frames_left);
                if ((value & 4u) == 0)
                    play_block(processor, left, right, midi, frames, true);
                else
                    play_block_with_the_player_held(processor, left, right, midi, frames,
                                                    (value & 8u) == 0);
                break;
            }
            case 2:
                if ((value & 4u) == 0) {
                    processor.reset();
                }
                else {
                    // A block of as many channels as the next byte says, of the
                    // three a host could hand over -- none, one, or the two the
                    // plugin asked for -- and with its highest bit, one whose
                    // channels are nowhere.
                    const std::uint8_t how = input.byte();
                    play_block_of_channels(processor, left, right, midi,
                                           std::min(static_cast<unsigned>(buffered), frames_left),
                                           how % 3u, (how & 0x80u) != 0);
                }
                break;
            default:
                // All notes off, on every channel, as a panic sends it.
                for (unsigned channel = 0; channel < 16; ++channel) {
                    const std::uint8_t message[3] {
                        static_cast<std::uint8_t>(0xb0u | channel), 123, 0};
                    midi.addEvent(message, 3, 0);
                }
                break;
            }
            break;
        }
    }

    processor.releaseResources();
    return 0;
}
