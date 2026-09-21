// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// Offline render of a fixed MIDI sequence through a VST3 build of the plugin.
//
//     ADLplug_render <plugin.vst3> <output.f32> [seconds] [warm-up ms]
//                    [--editor] [--snapshot <file.png>] [--no-teardown]
//                    [--state <file>] [--restore <file>] [--emulator <number>]
//                    [--save-hashes <file>] [checks]
//     ADLplug_render --compare <hashes file> <output hash> <state hash>
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
// so the teardown also covers what the editor allocates. --snapshot <file.png>
// does the same and, on Windows, saves a picture of the editor's window
// before closing it. --no-teardown exits
// straight after the summary, which separates a teardown problem from a
// rendering one. Timestamped progress goes to stderr, so a stall shows where
// it happened.
//
// --state <file> writes the saved state as it stands after the warm-up -- the
// data behind the state hash -- so that two builds whose hashes differ can be
// compared field by field.
//
// --restore <file> loads a state, such as one written by --state, before the
// warm-up. --emulator <number> then selects an emulator the way a project
// saved with it does: the plug-in's state is restored with the emulator number
// in its chip settings changed. Both happen on this thread before the warm-up,
// so renders with other emulators are reproducible as well. The summary names
// the emulator of the plug-in's parameter. For a number the build lacks, the
// parameter keeps the number, shown as "<Reserved n>", while another emulator
// plays (sources/plugin_state.h).
//
// For the tests in tests/, the exit status can report a result. --save-hashes
// <file> writes the output and state hashes, as "<output> <state>" in
// hexadecimal. The checks run after the summary: --expect-output <hash> and
// --expect-state <hash> compare a hash with a value, --expect-output-of <file>
// and --expect-state-of <file> with a file from --save-hashes,
// --require-emulator <name> compares the emulator the plug-in reports, and
// --require-sound requires output that is not silent and has only finite
// samples. --prepare-again then releases the plug-in, gives each parameter a
// pseudo-random value and prepares the plug-in again, as auval does with an
// Audio Unit: every parameter has to keep the value it was given, and the
// state has to stay the same. --upstream-parameters <OPL3|OPN2> requires the
// parameters of upstream ADLplug 1 to come first, in their order and with
// their VST3 IDs, which projects saved with upstream refer to. A failed check
// is printed and makes the exit status 1, after the teardown has run as usual.
// --compare checks a file from
// --save-hashes against given hashes without loading any plug-in; "-" skips a
// hash.

#include <juce_audio_processors/juce_audio_processors.h>

#if defined(JUCE_LINUX)
 #include "x_errors.h"
#endif

#if defined(JUCE_WINDOWS)
 #include <windows.h>
#endif

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <system_error>
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

// The IDs of the parameters that upstream ADLplug 1 gave its hosts, in their
// order: sources/opl3/parameter_block.cc and sources/opn2/parameter_block.cc of
// its last commit, a488abe.
std::vector<juce::String> upstream_parameter_ids(const std::string &chip)
{
    const bool opl3 = chip == "OPL3";
    std::vector<juce::String> ids {"mastervol", "emulator", "nchip", opl3 ? "n4op" : "chiptype"};
    static constexpr const char *opl3_operators[4] {"c1", "m1", "c2", "m2"};
    static constexpr const char *opn2_operators[4] {"op1", "op3", "op2", "op4"};
    for (int part = 1; part <= 16; ++part) {
        for (std::size_t op = 0; op < 4; ++op)
            ids.push_back("P" + juce::String(part) + (opl3 ? opl3_operators[op] : opn2_operators[op]) + "level");
    }
    if (opl3)
        ids.insert(ids.end(), {"volmodel", "deeptrem", "deepvib"});
    else
        ids.insert(ids.end(), {"volmodel", "lfoenable", "lfofreq"});
    return ids;
}

void pump_messages(int milliseconds)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(milliseconds);
}

void print_line(const std::string &text)
{
    // The line is flushed as it is written: what this tool says is read while it
    // renders, and a run that is stopped part way must have said what it said.
    std::cout << text << '\n' << std::flush;
}

constexpr std::uint64_t fnv1a64_basis = 14695981039346656037ull;

void fnv1a64_add(std::uint64_t &hash, std::span<const char> bytes)
{
    for (const char byte : bytes) {
        hash ^= static_cast<unsigned char>(byte);
        hash *= 1099511628211ull;
    }
}

std::uint64_t state_hash(juce::AudioPluginInstance &plugin)
{
    juce::MemoryBlock state;
    plugin.getStateInformation(state);
    std::uint64_t hash = fnv1a64_basis;
    fnv1a64_add(hash, {state.begin(), state.getSize()});
    return hash;
}

// Hashes are written and read as 16 hexadecimal digits.
std::string hash_text(std::uint64_t hash)
{
    char text[17];
    std::snprintf(text, sizeof text, "%016llx", static_cast<unsigned long long>(hash));
    return text;
}

std::optional<std::uint64_t> parse_hash(const std::string &text)
{
    std::uint64_t value = 0;
    const char *const end = text.data() + text.size();
    const auto [last, error] = std::from_chars(text.data(), end, value, 16);
    if (text.empty() || error != std::errc{} || last != end)
        return std::nullopt;
    return value;
}

// Reads "<output hash> <state hash>" from a file written by --save-hashes.
bool read_hashes(const std::string &path, std::uint64_t &output, std::uint64_t &state)
{
    std::ifstream in(path);
    std::string output_text, state_text;
    if (!(in >> output_text >> state_text))
        return false;
    const std::optional<std::uint64_t> output_hash = parse_hash(output_text);
    const std::optional<std::uint64_t> saved_state_hash = parse_hash(state_text);
    if (!output_hash || !saved_state_hash)
        return false;
    output = *output_hash;
    state = *saved_state_hash;
    return true;
}

// Sets the emulator number in the plug-in's state as the host keeps it.
// VST3PluginInstance saves copyXmlToBinary() of a <VST3PluginState> element
// whose children hold, in base64, the streams of the component and of the
// controller. A stream from the plug-in starts with copyXmlToBinary() of its
// <ADLMIDI-state>, and JUCE's VST3 wrapper may add data of its own after that,
// which is kept.
bool set_emulator_in_state(juce::MemoryBlock &state, int emulator)
{
    const std::unique_ptr<juce::XmlElement> host =
        juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    if (host == nullptr)
        return false;

    bool changed = false;
    for (juce::XmlElement *element : host->getChildIterator()) {
        juce::MemoryBlock stream;
        if (!stream.fromBase64Encoding(element->getAllSubText()) || stream.getSize() <= 8)
            continue;
        const std::unique_ptr<juce::XmlElement> own =
            juce::AudioProcessor::getXmlFromBinary(stream.getData(), static_cast<int>(stream.getSize()));
        const juce::XmlElement *const chip = (own != nullptr) ? own->getChildByName("chip") : nullptr;
        juce::XmlElement *const value = (chip != nullptr) ? chip->getChildByAttribute("name", "emulator") : nullptr;
        if (value == nullptr)
            continue;
        value->setAttribute("val", emulator);

        // copyXmlToBinary() writes a magic number, the length of the text, the
        // text and a terminating zero.
        const char *const bytes = stream.begin();
        const std::size_t own_size = 9u + juce::ByteOrder::littleEndianInt(bytes + 4);
        juce::MemoryBlock patched;
        juce::AudioProcessor::copyXmlToBinary(*own, patched);
        if (own_size < stream.getSize())
            patched.append(bytes + own_size, stream.getSize() - own_size);

        element->deleteAllTextElements();
        element->addTextElement(patched.toBase64Encoding());
        changed = true;
    }

    if (changed)
        juce::AudioProcessor::copyXmlToBinary(*host, state);
    return changed;
}

#if defined(JUCE_WINDOWS)
// A hosted editor draws into the plug-in's own native window, which
// Component::createComponentSnapshot() does not see: it gives a black picture.
// PrintWindow() has Windows draw the whole window, child windows included.
juce::Image capture_window(juce::Component &window)
{
    const auto hwnd = static_cast<HWND>(window.getWindowHandle());
    RECT rect {};
    if (hwnd == nullptr || GetClientRect(hwnd, &rect) == FALSE)
        return {};
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0)
        return {};

    const HDC screen = GetDC(nullptr);
    const HDC memory = CreateCompatibleDC(screen);
    const HBITMAP bitmap = CreateCompatibleBitmap(screen, width, height);
    const HGDIOBJ previous = SelectObject(memory, bitmap);
    // PW_CLIENTONLY | PW_RENDERFULLCONTENT: the client area, without the frame,
    // and with what is drawn through DirectX.
    const bool printed = PrintWindow(hwnd, memory, 1 | 2) != FALSE;

    BITMAPINFO info {};
    info.bmiHeader.biSize = static_cast<DWORD>(sizeof(BITMAPINFOHEADER));
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;  // rows from the top
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    std::vector<std::uint8_t> pixels(4 * static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    const bool copied = printed &&
        GetDIBits(memory, bitmap, 0, static_cast<UINT>(height), pixels.data(), &info, DIB_RGB_COLORS) == height;

    SelectObject(memory, previous);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);
    if (!copied)
        return {};

    juce::Image picture(juce::Image::RGB, width, height, false, juce::SoftwareImageType());
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t i = 4 * (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x));
            picture.setPixelAt(x, y, juce::Colour(pixels[i + 2], pixels[i + 1], pixels[i]));
        }
    }
    return picture;
}
#endif

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
    // The time of a step is worked out from the number of the step, so that how
    // many steps there are is not a matter of what a sum of doubles comes to.
    for (int step = 0; at(0.25 * step) < end_of_notes; ++step) {
        const double t = 0.25 * step;
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
        for (int step = 0; at(0.1 * ch + 0.5 * step) < end_of_notes; ++step) {
            const double t = 0.1 * ch + 0.5 * step;
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
    for (int step = 0; at(0.125 * step) < end_of_notes; ++step) {
        const double t = 0.125 * step;
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
    if (argc == 5 && std::strcmp(argv[1], "--compare") == 0) {
        std::uint64_t output = 0;
        std::uint64_t state = 0;
        if (!read_hashes(argv[2], output, state)) {
            print_line(std::string("error: cannot read hashes from ") + argv[2]);
            return 2;
        }
        bool same = true;
        const auto compare = [&same](const char *what, std::uint64_t actual, const std::string &expected) {
            if (expected == "-")
                return;
            const bool equal = parse_hash(expected) == actual;
            print_line(std::string(what) + " " + hash_text(actual) + (equal ? " as expected" : ", expected " + expected));
            same = same && equal;
        };
        compare("output", output, argv[3]);
        compare("state", state, argv[4]);
        return same ? 0 : 1;
    }

    std::vector<std::string> args;
    bool open_editor = false;
    bool teardown = true;
    std::string state_file;
    std::string restore_file;
    std::string snapshot_file;
    int emulator_number = -1;
    std::string save_hashes_file;
    std::optional<std::uint64_t> expected_output;
    std::optional<std::uint64_t> expected_state;
    std::string expected_output_file;
    std::string expected_state_file;
    std::string required_emulator;
    bool require_sound = false;
    bool prepare_again = false;
    std::string upstream_chip;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--editor")
            open_editor = true;
        else if (arg == "--snapshot" && i + 1 < argc)
            snapshot_file = argv[++i];
        else if (arg == "--no-teardown")
            teardown = false;
        else if (arg == "--state" && i + 1 < argc)
            state_file = argv[++i];
        else if (arg == "--restore" && i + 1 < argc)
            restore_file = argv[++i];
        else if (arg == "--emulator" && i + 1 < argc)
            emulator_number = juce::String(argv[++i]).getIntValue();
        else if (arg == "--save-hashes" && i + 1 < argc)
            save_hashes_file = argv[++i];
        else if ((arg == "--expect-output" || arg == "--expect-state") && i + 1 < argc) {
            const std::optional<std::uint64_t> value = parse_hash(argv[++i]);
            if (!value) {
                print_line("error: " + arg + " needs a hash in hexadecimal");
                return 2;
            }
            (arg == "--expect-output" ? expected_output : expected_state) = value;
        }
        else if (arg == "--expect-output-of" && i + 1 < argc)
            expected_output_file = argv[++i];
        else if (arg == "--expect-state-of" && i + 1 < argc)
            expected_state_file = argv[++i];
        else if (arg == "--require-emulator" && i + 1 < argc)
            required_emulator = argv[++i];
        else if (arg == "--require-sound")
            require_sound = true;
        else if (arg == "--prepare-again")
            prepare_again = true;
        else if (arg == "--upstream-parameters" && i + 1 < argc) {
            upstream_chip = argv[++i];
            if (upstream_chip != "OPL3" && upstream_chip != "OPN2") {
                print_line("error: --upstream-parameters needs OPL3 or OPN2");
                return 2;
            }
        }
        else
            args.push_back(arg);
    }
    if (args.size() < 2) {
        print_line("usage: ADLplug_render <plugin.vst3> <output.f32> [seconds] [warm-up ms] [--editor] "
                   "[--snapshot <file.png>] [--no-teardown] [--state <file>] [--restore <file>] [--emulator <number>] "
                   "[--save-hashes <file>] [--expect-output <hash>] [--expect-state <hash>] [--expect-output-of <file>] "
                   "[--expect-state-of <file>] [--require-emulator <name>] [--require-sound] [--prepare-again] "
                   "[--upstream-parameters <OPL3|OPN2>]\n"
                   "       ADLplug_render --compare <hashes file> <output hash|-> <state hash|->");
        return 2;
    }

    // Hashes expected from files are read now, so a missing file stops the run
    // before anything is loaded.
    std::uint64_t file_output = 0;
    std::uint64_t file_state = 0;
    if (!expected_output_file.empty()) {
        if (!read_hashes(expected_output_file, file_output, file_state)) {
            print_line("error: cannot read hashes from " + expected_output_file);
            return 2;
        }
        expected_output = file_output;
    }
    if (!expected_state_file.empty()) {
        if (!read_hashes(expected_state_file, file_output, file_state)) {
            print_line("error: cannot read hashes from " + expected_state_file);
            return 2;
        }
        expected_state = file_state;
    }
    const double seconds = args.size() > 2 ? juce::String(args[2]).getDoubleValue() : 20.0;
    const int warmup_ms = args.size() > 3 ? juce::String(args[3]).getIntValue() : 5000;

    int exit_status = 0;
    milestone("start");
   #if defined(JUCE_LINUX)
    report_x_errors();
   #endif
    {
        const juce::ScopedJuceInitialiser_GUI juce_init;
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

        // Hosted, the parameters of a VST3 plug-in have its ParamIDs for IDs.
        std::vector<std::string> upstream_failures;
        if (!upstream_chip.empty()) {
            const std::vector<juce::String> ids = upstream_parameter_ids(upstream_chip);
            const juce::Array<juce::AudioProcessorParameter *> &parameters = plugin->getParameters();
            constexpr std::size_t listed = 8;
            std::size_t wrong = 0;
            for (std::size_t i = 0; i < ids.size(); ++i) {
                const juce::String expected(juce::VST3ClientExtensions::convertJuceParameterId(ids[i]));
                const int index = static_cast<int>(i);
                const auto *hosted = (index < parameters.size())
                    ? dynamic_cast<juce::HostedAudioProcessorParameter *>(parameters[index]) : nullptr;
                const juce::String actual = (hosted != nullptr) ? hosted->getParameterID() : juce::String("none");
                if (actual != expected && ++wrong <= listed)
                    upstream_failures.push_back("parameter " + std::to_string(i) + " has the ID " + actual.toStdString() +
                                                ", where " + ids[i].toStdString() + " of upstream has " +
                                                expected.toStdString());
            }
            if (wrong > listed)
                upstream_failures.push_back(std::to_string(wrong - listed) + " more parameters differ from upstream");
            milestone("compared " + std::to_string(ids.size()) + " parameters with those of upstream");
        }

        plugin->enableAllBuses();
        plugin->prepareToPlay(sample_rate, block_size);
        milestone("prepared");

        if (!restore_file.empty()) {
            juce::MemoryBlock restored;
            const juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile(juce::String(restore_file));
            if (!file.loadFileAsData(restored)) {
                print_line("error: cannot read " + restore_file);
                return 1;
            }
            plugin->setStateInformation(restored.getData(), static_cast<int>(restored.getSize()));
            milestone("state restored from " + restore_file);
        }

        if (emulator_number >= 0) {
            juce::MemoryBlock current;
            plugin->getStateInformation(current);
            if (!set_emulator_in_state(current, emulator_number)) {
                print_line("error: the plug-in's state has no emulator setting");
                return 1;
            }
            plugin->setStateInformation(current.getData(), static_cast<int>(current.getSize()));
            milestone("emulator " + std::to_string(emulator_number) + " selected through the state");
        }

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

        if (!state_file.empty()) {
            juce::MemoryBlock saved;
            plugin->getStateInformation(saved);
            std::ofstream state_out(state_file, std::ios::binary);
            state_out.write(saved.begin(), static_cast<std::streamsize>(saved.getSize()));
            if (!state_out) {
                print_line("error: cannot write " + state_file);
                return 1;
            }
        }

        std::string emulator = "?";
        for (const auto *parameter : plugin->getParameters())
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
                    const auto bytes = std::bit_cast<std::array<char, sizeof(float)>>(sample);
                    out.write(bytes.data(), bytes.size());
                    fnv1a64_add(hash, bytes);
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

        if (!save_hashes_file.empty()) {
            std::ofstream hashes_out(save_hashes_file);
            hashes_out << hash_text(hash) << ' ' << hash_text(state) << '\n';
            if (!hashes_out) {
                print_line("error: cannot write " + save_hashes_file);
                return 1;
            }
        }

        std::vector<std::string> failures = upstream_failures;
        if (expected_output && *expected_output != hash)
            failures.push_back("output " + hash_text(hash) + ", expected " + hash_text(*expected_output));
        if (expected_state && *expected_state != state)
            failures.push_back("state " + hash_text(state) + ", expected " + hash_text(*expected_state));
        if (!required_emulator.empty() && emulator != required_emulator)
            failures.push_back("emulator '" + emulator + "', expected '" + required_emulator + "'");
        if (require_sound && (peak <= 0.0f || nonfinite != 0))
            failures.emplace_back("the output is silent or has samples that are not finite");
        if (prepare_again) {
            plugin->releaseResources();
            const juce::Array<juce::AudioProcessorParameter *> &parameters = plugin->getParameters();
            Sequence_Rng rng(0x5eed);
            for (juce::AudioProcessorParameter *parameter : parameters)
                parameter->setValueNotifyingHost(static_cast<float>(rng.below(1001)) / 1000.0f);
            std::vector<float> given;
            for (const juce::AudioProcessorParameter *parameter : parameters)
                given.push_back(parameter->getValue());
            // Saving the state also hands the new values over to the plug-in.
            const std::uint64_t released_state = state_hash(*plugin);
            plugin->prepareToPlay(sample_rate, block_size);
            pump_messages(100);
            constexpr int listed = 8;
            int changed = 0;
            for (int i = 0; i < parameters.size(); ++i) {
                const float value = parameters[i]->getValue();
                const float before = given[static_cast<std::size_t>(i)];
                if (value != before && ++changed <= listed)
                    failures.push_back("parameter '" + parameters[i]->getName(64).toStdString() + "' is " +
                                       std::to_string(value) + " after preparing again, " + std::to_string(before) +
                                       " before");
            }
            if (changed > listed)
                failures.push_back(std::to_string(changed - listed) + " more parameters changed when prepared again");
            if (state_hash(*plugin) != released_state)
                failures.emplace_back("the state changed when the plug-in was prepared again");
            milestone("released, " + std::to_string(parameters.size()) + " parameters set, prepared again");
        }
        for (const std::string &failure : failures)
            print_line("check failed: " + failure);
        if (!failures.empty())
            exit_status = 1;

        if (open_editor || !snapshot_file.empty()) {
            std::unique_ptr<juce::AudioProcessorEditor> editor(plugin->createEditorAndMakeActive());
            if (editor == nullptr) {
                print_line("error: the plug-in did not create an editor");
                return 1;
            }
            editor->setTopLeftPosition(60, 60);
            editor->addToDesktop(juce::ComponentPeer::windowHasTitleBar);
            editor->setVisible(true);
            pump_messages(1500);
            if (!snapshot_file.empty()) {
               #if defined(JUCE_WINDOWS)
                const juce::Image picture = capture_window(*editor);
               #else
                const juce::Image picture;  // no capture of native windows elsewhere yet
               #endif
                const juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile(juce::String(snapshot_file));
                bool written = false;
                if (picture.isValid() && file.deleteFile()) {
                    juce::FileOutputStream stream(file);
                    written = stream.openedOk() && juce::PNGImageFormat().writeImageToStream(picture, stream);
                }
                if (!written) {
                    print_line("error: cannot capture the editor into " + snapshot_file);
                    return 1;
                }
            }
            editor.reset();
            milestone("editor opened and closed");
            pump_messages(200);
        }

        if (!teardown) {
            milestone("exiting without teardown");
            std::_Exit(exit_status);
        }

        plugin->releaseResources();
        milestone("releaseResources returned");
        pump_messages(100);
        plugin.reset();
        milestone("plug-in destroyed");
        pump_messages(100);
    }
    milestone("JUCE shut down");
    return exit_status;
}
