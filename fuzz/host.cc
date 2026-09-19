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
//   byte 0  the sample rate, by its place in the rates hosts use
//   byte 1  the greatest number of samples in a block, the same way
//
// A record is one byte, and the bits at its top say what it is:
//
//   00xxxxxx  a MIDI message of 1 + (x & 3) bytes, which follow, at the sample
//             (x >> 2) & 15 of the next block. Bytes that are not a message are
//             dropped by the buffer, as they are by a host's
//   010xxxxx  a block of 1 + x * 32 samples, or of the whole block if that is
//             fewer. The plugin cuts a block into segments of its own, so the
//             number matters as much as the messages in it
//   011xxxxx  a note on the channel x & 15: the note and the velocity follow,
//             seven bits of each. A message of three bytes says the same, but
//             this way a note is one record and the fuzzer reaches the
//             synthesis instead of stopping at the status bytes
//   10xxxxxx  automation: two bytes follow, the first naming one of the
//             plugin's parameters with the six bits here, the second giving it a
//             value over the whole of its range
//   11xxxxxx  what else a host does: prepare the plugin again (which is where it
//             carries its state over), a bypassed block, reset, and all notes
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
#include <vector>

namespace {

// The audio of one input, in frames, and how many records it may have.
constexpr unsigned frames_max = 4096;
constexpr unsigned records_max = 512;

// What hosts ask for. The first of each is what an input that says nothing gets.
constexpr double rates[] {44100.0, 22050.0, 32000.0, 48000.0, 88200.0, 96000.0, 192000.0};
constexpr int sizes[] {512, 1, 16, 64, 128, 256, 1024, 2048};

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

    const double rate = rates[input.byte() % std::size(rates)];
    const int block = sizes[input.byte() % std::size(sizes)];
    processor.prepareToPlay(rate, block);

    std::vector<float> left(static_cast<std::size_t>(block));
    std::vector<float> right(static_cast<std::size_t>(block));
    juce::MidiBuffer midi;
    unsigned frames_left = frames_max;
    unsigned records_left = records_max;

    while (!input.done() && records_left-- > 0) {
        const std::uint8_t record = input.byte();
        const unsigned value = record & 0x3fu;

        switch (record >> 6) {
        case 0: {
            const std::vector<std::uint8_t> message = input.bytes(1u + (value & 3u));
            if (!message.empty())
                midi.addEvent(message.data(), static_cast<int>(message.size()),
                              static_cast<int>((value >> 2) & 15u));
            break;
        }
        case 1:
            if ((record & 0x20u) == 0) {
                const unsigned frames = std::min({static_cast<unsigned>(block),
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
            const float to = static_cast<float>(input.byte()) / 255.0f;
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
                processor.releaseResources();
                processor.prepareToPlay(rate, block);
                break;
            case 1:
                play_block(processor, left, right, midi,
                           std::min(static_cast<unsigned>(block), frames_left), true);
                break;
            case 2:
                processor.reset();
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
