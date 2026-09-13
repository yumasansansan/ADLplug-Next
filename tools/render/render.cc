//          Part of ADLplug, distributed under the GNU GPL v3.
//               (See accompanying file LICENSE.)
//
// Offline render of a fixed MIDI sequence through a VST3 build of the plugin.
//
//     ADLplug_render <plugin.vst3> <output.f32> [seconds] [warm-up ms]
//                    [--editor] [--no-teardown]
//
// Writes the plugin's output as interleaved 32-bit float samples and prints a
// one-line summary with a hash, so two builds -- Debug against Release with
// LTO, say -- can be compared sample for sample. Loading through the VST3
// interface also exercises the shipped module's exports and factory, and the
// teardown at the end unloads the module as a host would.
//
// Three things keep runs reproducible:
//
//  * Program changes happen in a warm-up phase that is not recorded. Selecting
//    a program updates the instrument parameters, which makes the plugin ask
//    its worker thread to re-measure the instrument; the result reaches the
//    voice allocator whenever the worker finishes. The warm-up paces silent
//    blocks in real time, as a live host would, so that has settled before
//    anything is recorded.
//  * The warm-up is a fixed number of blocks, not a fixed time. The chips'
//    LFOs and envelope counters advance with every sample, so recordings only
//    start from the same chip state if the same number of samples came first.
//  * The message loop is pumped. Host and plug-in share this thread as their
//    message thread, and neither may be starved of it.
//
// The measured delays are part of the plug-in's saved state, so the summary
// reports a hash of that state and the warm-up block after which it last
// changed. If that is near the end of the warm-up, the warm-up was too short.
//
// --editor opens the editor in a window after rendering and closes it again,
// so the teardown also covers what the editor allocates. --no-teardown exits
// straight after the summary, which separates a teardown problem from a
// rendering one. Timestamped progress goes to stderr, so a stall shows where
// it happened.

#include <juce_audio_processors/juce_audio_processors.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr double sample_rate = 48000.0;
constexpr int block_size = 512;
constexpr int state_check_interval = 16;   // warm-up blocks between state hashes

using Clock = std::chrono::steady_clock;
const Clock::time_point start_time = Clock::now();

void milestone(const std::string &what)
{
    const double t = std::chrono::duration<double>(Clock::now() - start_time).count();
    std::fprintf(stderr, "[%8.3f s] %s", t, what.c_str());
    std::fputc(10, stderr);
    std::fflush(stderr);
}

void pump_messages(int milliseconds)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(milliseconds);
}

void print_line(const std::string &text)
{
    std::cout << text << std::endl;
}

constexpr std::uint64_t fnv1a64_basis = 14695981039346656037ull;

void fnv1a64_add(std::uint64_t &hash, const void *data, std::size_t size)
{
    const auto *bytes = static_cast<const unsigned char *>(data);
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
}

std::uint64_t state_hash(juce::AudioPluginInstance &plugin)
{
    juce::MemoryBlock state;
    plugin.getStateInformation(state);
    std::uint64_t hash = fnv1a64_basis;
    fnv1a64_add(hash, state.getData(), state.getSize());
    return hash;
}

struct Scheduled_Event {
    int sample = 0;
    juce::MidiMessage message;
};

// Channel and program, applied during warm-up.
constexpr int part_programs[][2] = { {1, 0}, {2, 19}, {3, 33}, {4, 81} };

// Fixed-seed generator whose output does not depend on the standard library.
class Sequence_Rng {
public:
    explicit Sequence_Rng(std::uint64_t seed) : state_(seed) {}

    int below(int n)
    {
        state_ = state_ * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<int>((state_ >> 33) % static_cast<std::uint64_t>(n));
    }

private:
    std::uint64_t state_;
};

std::vector<Scheduled_Event> make_sequence(double seconds)
{
    std::vector<Scheduled_Event> ev;
    Sequence_Rng rng(0x0AD1F00Dull);

    const auto at = [](double t) { return static_cast<int>(t * sample_rate); };
    const int total = at(seconds);
    const int end_of_notes = total - std::min(at(3.0), total / 4);   // leave time for releases
    const auto add = [&ev](int sample, const juce::MidiMessage &m) { ev.push_back({sample, m}); };
    const auto velocity = [](int v) { return static_cast<juce::uint8>(v); };

    // Channel 1: overlapping random chords -- more voices than the chips have
    // channels, so the voice allocator has to steal.
    for (double t = 0.0; at(t) < end_of_notes; t += 0.25) {
        const int voices = 3 + rng.below(3);
        for (int v = 0; v < voices; ++v) {
            const int note = 40 + rng.below(40);
            const int on = at(t) + rng.below(200);
            const int off = std::min(on + at(0.1 + rng.below(160) / 100.0), end_of_notes);
            add(on, juce::MidiMessage::noteOn(1, note, velocity(30 + rng.below(98))));
            // Every fourth release is a note-on with velocity 0.
            add(off, rng.below(4) == 0 ? juce::MidiMessage::noteOn(1, note, velocity(0))
                                       : juce::MidiMessage::noteOff(1, note));
        }
    }
    add(at(5.0), juce::MidiMessage::controllerEvent(1, 64, 127));
    add(at(8.0), juce::MidiMessage::controllerEvent(1, 64, 0));

    // Channels 2-4: a line each, with pitch bend, modulation, volume and pan.
    for (int ch = 2; ch <= 4; ++ch) {
        for (double t = 0.1 * ch; at(t) < end_of_notes; t += 0.5) {
            const int note = 48 + rng.below(30);
            const int on = at(t);
            add(on, juce::MidiMessage::noteOn(ch, note, velocity(60 + rng.below(60))));
            add(std::min(on + at(0.4), end_of_notes), juce::MidiMessage::noteOff(ch, note));
            add(on + at(0.1), juce::MidiMessage::pitchWheel(ch, rng.below(16384)));
            add(on + at(0.2), juce::MidiMessage::controllerEvent(ch, 1, rng.below(128)));
            add(on + at(0.3), juce::MidiMessage::controllerEvent(ch, 7, 64 + rng.below(64)));
            add(on + at(0.3), juce::MidiMessage::controllerEvent(ch, 10, rng.below(128)));
        }
    }

    // Channel 10: drums.
    for (double t = 0.0; at(t) < end_of_notes; t += 0.125) {
        const int note = 35 + rng.below(47);
        add(at(t), juce::MidiMessage::noteOn(10, note, velocity(40 + rng.below(88))));
        add(at(t) + at(0.1), juce::MidiMessage::noteOff(10, note));
    }

    std::stable_sort(ev.begin(), ev.end(),
                     [](const Scheduled_Event &a, const Scheduled_Event &b) { return a.sample < b.sample; });
    return ev;
}

} // namespace

int main(int argc, char *argv[])
{
    std::vector<std::string> args;
    bool open_editor = false;
    bool teardown = true;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--editor")
            open_editor = true;
        else if (arg == "--no-teardown")
            teardown = false;
        else
            args.push_back(arg);
    }
    if (args.size() < 2) {
        print_line("usage: ADLplug_render <plugin.vst3> <output.f32> [seconds] [warm-up ms] [--editor] [--no-teardown]");
        return 2;
    }
    const double seconds = args.size() > 2 ? juce::String(args[2]).getDoubleValue() : 20.0;
    const int warmup_ms = args.size() > 3 ? juce::String(args[3]).getIntValue() : 5000;

    milestone("start");
    {
        juce::ScopedJuceInitialiser_GUI juce_init;
        juce::AudioPluginFormatManager formats;
        formats.addFormat(std::make_unique<juce::VST3PluginFormat>());

        const juce::File plugin_file =
            juce::File::getCurrentWorkingDirectory().getChildFile(juce::String(args[0]));

        juce::OwnedArray<juce::PluginDescription> types;
        for (int i = 0; i < formats.getNumFormats(); ++i)
            formats.getFormat(i)->findAllTypesForFile(types, plugin_file.getFullPathName());
        if (types.isEmpty()) {
            print_line("error: no VST3 plug-in found in " + plugin_file.getFullPathName().toStdString());
            return 1;
        }

        juce::String error;
        std::unique_ptr<juce::AudioPluginInstance> plugin =
            formats.createPluginInstance(*types[0], sample_rate, block_size, error);
        if (plugin == nullptr) {
            print_line("error: could not instantiate the plug-in: " + error.toStdString());
            return 1;
        }
        milestone("plug-in instantiated");

        plugin->enableAllBuses();
        plugin->prepareToPlay(sample_rate, block_size);
        milestone("prepared");

        const std::string plugin_name = types[0]->name.toStdString();
        const int out_channels = plugin->getTotalNumOutputChannels();
        const int buffer_channels = std::max(out_channels, plugin->getTotalNumInputChannels());

        juce::AudioBuffer<float> buffer(buffer_channels, block_size);
        juce::MidiBuffer midi;

        // Warm-up: select the programs, then process a fixed number of silent
        // blocks, paced in real time so the worker's re-measurements arrive
        // before anything is recorded.
        for (const auto &part : part_programs)
            midi.addEvent(juce::MidiMessage::programChange(part[0], part[1]), 0);
        const int warm_blocks = static_cast<int>(std::ceil(warmup_ms * sample_rate / (1000.0 * block_size)));
        const std::chrono::duration<double> block_period(block_size / sample_rate);
        const Clock::time_point warm_start = Clock::now();
        std::uint64_t state = state_hash(*plugin);
        int settled_after = 0;
        for (int b = 1; b <= warm_blocks; ++b) {
            buffer.clear();
            plugin->processBlock(buffer, midi);
            midi.clear();
            if (b % state_check_interval == 0 || b == warm_blocks) {
                const std::uint64_t current = state_hash(*plugin);
                if (current != state) {
                    state = current;
                    settled_after = b;
                }
            }
            const Clock::time_point due =
                warm_start + std::chrono::duration_cast<Clock::duration>(block_period * b);
            do
                pump_messages(1);
            while (Clock::now() < due);
        }
        milestone("warm-up done: " + std::to_string(warm_blocks) + " blocks, state last changed after block " +
                  std::to_string(settled_after));

        std::string emulator = "?";
        for (auto *parameter : plugin->getParameters())
            if (parameter->getName(64) == "Emulator")
                emulator = parameter->getCurrentValueAsText().toStdString();

        const std::vector<Scheduled_Event> events = make_sequence(seconds);

        std::ofstream out(args[1], std::ios::binary);
        if (!out) {
            print_line("error: cannot write " + args[1]);
            return 1;
        }

        std::size_t next_event = 0;
        const int total = static_cast<int>(sample_rate * seconds);
        std::uint64_t hash = fnv1a64_basis;
        float peak = 0.0f;
        double sum_squares = 0.0;
        unsigned long long nonfinite = 0;
        int block_index = 0;

        for (int pos = 0; pos < total; pos += block_size, ++block_index) {
            const int frames = std::min(block_size, total - pos);
            buffer.setSize(buffer_channels, frames, false, false, true);
            buffer.clear();
            midi.clear();
            while (next_event < events.size() && events[next_event].sample < pos + frames) {
                midi.addEvent(events[next_event].message, events[next_event].sample - pos);
                ++next_event;
            }

            plugin->processBlock(buffer, midi);

            for (int i = 0; i < frames; ++i) {
                for (int c = 0; c < out_channels; ++c) {
                    const float sample = buffer.getSample(c, i);
                    char bytes[sizeof(float)];
                    std::memcpy(bytes, &sample, sizeof(float));
                    out.write(bytes, sizeof(float));
                    fnv1a64_add(hash, bytes, sizeof(float));
                    if (!std::isfinite(sample))
                        ++nonfinite;
                    peak = std::max(peak, std::abs(sample));
                    sum_squares += static_cast<double>(sample) * static_cast<double>(sample);
                }
            }

            if (block_index % 64 == 0)
                pump_messages(0);
        }
        out.close();
        milestone("rendered " + std::to_string(total) + " frames and closed the output");

        const double samples_total = static_cast<double>(total) * static_cast<double>(std::max(out_channels, 1));
        char summary[400];
        std::snprintf(summary, sizeof summary,
                      "plugin=%s emulator=%s channels=%d frames=%d peak=%.6f rms=%.6f nonfinite=%llu "
                      "warmup=%d settled=%d state=%016llx fnv1a64=%016llx",
                      plugin_name.c_str(), emulator.c_str(), out_channels, total,
                      static_cast<double>(peak), std::sqrt(sum_squares / samples_total), nonfinite,
                      warm_blocks, settled_after, static_cast<unsigned long long>(state),
                      static_cast<unsigned long long>(hash));
        print_line(summary);

        if (open_editor) {
            std::unique_ptr<juce::AudioProcessorEditor> editor(plugin->createEditorAndMakeActive());
            if (editor == nullptr) {
                print_line("error: the plug-in did not create an editor");
                return 1;
            }
            editor->setTopLeftPosition(60, 60);
            editor->addToDesktop(juce::ComponentPeer::windowHasTitleBar);
            editor->setVisible(true);
            pump_messages(1500);
            editor.reset();
            milestone("editor opened and closed");
            pump_messages(200);
        }

        if (!teardown) {
            milestone("exiting without teardown");
            std::_Exit(0);
        }

        plugin->releaseResources();
        milestone("releaseResources returned");
        pump_messages(100);
        plugin.reset();
        milestone("plug-in destroyed");
        pump_messages(100);
    }
    milestone("JUCE shut down");
    return 0;
}
