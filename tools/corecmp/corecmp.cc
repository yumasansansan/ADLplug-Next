// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// How close each emulator core comes to a reference core, by default the
// low-level one, which was drawn from a die shot of the chip.
//
//     ADLplug_corecmp [--bank <file.wopl>] [--reference <emulator>]
//                     [--cores <emulator>,...] [--scenes <word>,...]
//                     [--chips <n>] [--jobs <n>] [--csv <file>]
//                     [--dump <directory>] [--warm-up <frames>] [--soft-pan]
//
// Plays the same MIDI through libADLMIDI with each core, one after another,
// and compares what each core makes with what the reference makes. It plays at
// the chip's own rate (ADL_CHIP_SAMPLE_RATE), as the plug-in does: there the
// library hands over each sample as the core made it, with nothing
// interpolated, so the difference between two cores is theirs alone. Every
// scene starts from a player of its own, so that each core starts it from its
// own state after a reset, and a difference in one scene does not carry into
// the next. Before a scene the player runs silent for a while (--warm-up, 8192
// frames by default), which is not compared: setting up the player and the bank
// writes a thousand registers or so, and a core that takes its writes as the
// chip's bus does, as the low-level one does at 2 2/9 samples each, is
// still taking them when the scene would begin. The panning is the chip's, each
// output on or off; --soft-pan asks for libADLMIDI's own, which the low-level
// core does not have.
//
// For each core and scene it reports, over both outputs:
//
//  * the error, the energy of the reference over the energy of the difference,
//    in decibels (higher is closer; "same" when there is no difference at all);
//  * the largest difference, in steps of the 16-bit sample;
//  * how many samples are the same;
//  * the shift and the gain, the number of samples by which to move the core's
//    output against the reference's (within 32) and the factor by which to
//    scale it to bring them closest, and the residual, the error that is left
//    then: this tells a core that is late or early, or louder or softer, from
//    one that makes other samples;
//  * how many times faster than real time the core played (with --jobs 1 only;
//    with more jobs the scenes share the processor).
//
// The scenes (--scenes chooses among them by name) are every melodic program
// of the bank on low, middle and high notes, every percussion note, chords of
// four programs, a note under pitch bends, vibrato, volume, expression and
// panning, and the velocities of one note.
//
// --dump writes each core's output of each scene to <directory>/<scene>.<emulator>.s32,
// as interleaved 32-bit integers in the machine's byte order, to look at.

#include <adlmidi.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

constexpr unsigned chip_rate = ADL_CHIP_SAMPLE_RATE;
constexpr int max_shift = 32;

struct Event {
    std::uint32_t frame = 0;
    std::uint8_t status = 0;
    std::uint8_t data1 = 0;
    std::uint8_t data2 = 0;
};

struct Scene {
    std::string name;
    std::uint32_t frames = 0;
    std::vector<Event> events;
};

std::uint32_t frames_of(double seconds)
{
    return static_cast<std::uint32_t>(std::lround(seconds * static_cast<double>(chip_rate)));
}

// A scene written as a list of MIDI messages at times in seconds.
class Scene_Builder {
public:
    explicit Scene_Builder(std::string name) { scene_.name = std::move(name); }

    Scene_Builder &after(double seconds) { now_ += frames_of(seconds); return *this; }
    Scene_Builder &message(std::uint8_t status, std::uint8_t data1, std::uint8_t data2 = 0)
    {
        scene_.events.push_back(Event{.frame = now_, .status = status, .data1 = data1, .data2 = data2});
        return *this;
    }
    Scene_Builder &program(unsigned channel, unsigned number)
        { return message(static_cast<std::uint8_t>(0xc0 | channel), static_cast<std::uint8_t>(number)); }
    Scene_Builder &note_on(unsigned channel, unsigned note, unsigned velocity)
        { return message(static_cast<std::uint8_t>(0x90 | channel), static_cast<std::uint8_t>(note), static_cast<std::uint8_t>(velocity)); }
    Scene_Builder &note_off(unsigned channel, unsigned note)
        { return message(static_cast<std::uint8_t>(0x80 | channel), static_cast<std::uint8_t>(note), 64); }
    Scene_Builder &controller(unsigned channel, unsigned number, unsigned value)
        { return message(static_cast<std::uint8_t>(0xb0 | channel), static_cast<std::uint8_t>(number), static_cast<std::uint8_t>(value)); }
    Scene_Builder &bend(unsigned channel, unsigned value)
    {
        return message(static_cast<std::uint8_t>(0xe0 | channel), static_cast<std::uint8_t>(value & 0x7f),
                       static_cast<std::uint8_t>((value >> 7) & 0x7f));
    }

    Scene done(double tail_seconds)
    {
        scene_.frames = now_ + frames_of(tail_seconds);
        std::stable_sort(scene_.events.begin(), scene_.events.end(),
                         [](const Event &a, const Event &b) { return a.frame < b.frame; });
        return std::move(scene_);
    }

private:
    Scene scene_;
    std::uint32_t now_ = 0;
};

std::vector<Scene> make_scenes()
{
    std::vector<Scene> scenes;
    // Every melodic program: a low, a middle and a high note, each held and let go.
    for (unsigned program = 0; program < 128; ++program) {
        Scene_Builder b("program-" + std::to_string(program));
        b.program(0, program);
        for (const unsigned note : {36u, 60u, 84u}) {
            b.after(0.02).note_on(0, note, 100).after(0.4).note_off(0, note).after(0.2);
        }
        scenes.push_back(b.done(0.1));
    }
    // Every percussion note of the General MIDI map.
    {
        Scene_Builder b("percussion");
        for (unsigned note = 35; note <= 81; ++note)
            b.after(0.02).note_on(9, note, 110).after(0.25).note_off(9, note).after(0.08);
        scenes.push_back(b.done(0.3));
    }
    // Chords of four programs at once, so that many slots play together.
    {
        Scene_Builder b("chords");
        const unsigned programs[4] = {0, 48, 32, 80};
        for (unsigned c = 0; c < 4; ++c)
            b.program(c, programs[c]);
        const unsigned roots[8] = {48, 53, 55, 50, 57, 52, 55, 48};
        for (const unsigned root : roots) {
            b.after(0.01);
            for (unsigned c = 0; c < 4; ++c)
                for (const unsigned step : {0u, 4u, 7u})
                    b.note_on(c, root + step + 12 * (c == 2 ? 0 : 1), 90);
            b.after(0.5);
            for (unsigned c = 0; c < 4; ++c)
                for (const unsigned step : {0u, 4u, 7u})
                    b.note_off(c, root + step + 12 * (c == 2 ? 0 : 1));
        }
        scenes.push_back(b.done(0.5));
    }
    // One note under pitch bends, vibrato, volume, expression and panning, and
    // the sustain pedal.
    {
        Scene_Builder b("expression");
        b.program(0, 48).after(0.02).note_on(0, 60, 100);
        for (unsigned i = 0; i <= 32; ++i)
            b.after(0.03).bend(0, std::min(16383u, i * 512));
        for (unsigned i = 0; i <= 32; ++i)
            b.after(0.03).bend(0, 16383 - std::min(16383u, i * 512));
        b.bend(0, 8192);
        for (unsigned i = 0; i <= 16; ++i)
            b.after(0.05).controller(0, 1, std::min(127u, i * 8));
        b.controller(0, 1, 0);
        for (unsigned i = 0; i <= 16; ++i)
            b.after(0.04).controller(0, 7, 127 - std::min(127u, i * 8));
        b.controller(0, 7, 100);
        for (unsigned i = 0; i <= 16; ++i)
            b.after(0.04).controller(0, 11, 127 - std::min(127u, i * 8));
        b.controller(0, 11, 127);
        for (unsigned i = 0; i <= 8; ++i)
            b.after(0.06).controller(0, 10, std::min(127u, i * 16));
        b.controller(0, 10, 64).controller(0, 64, 127).after(0.1).note_off(0, 60).after(0.6).controller(0, 64, 0);
        scenes.push_back(b.done(0.5));
    }
    // The velocities of one note.
    {
        Scene_Builder b("velocity");
        b.program(0, 0);
        for (unsigned velocity = 1; velocity <= 127; velocity += 14)
            b.after(0.02).note_on(0, 60, velocity).after(0.25).note_off(0, 60).after(0.08);
        scenes.push_back(b.done(0.3));
    }
    return scenes;
}

void play(ADL_MIDIPlayer *pl, const Event &e)
{
    const auto channel = static_cast<ADL_UInt8>(e.status & 0x0f);
    switch (e.status >> 4) {
    case 0x9:
        if (e.data2 != 0) {
            adl_rt_noteOn(pl, channel, e.data1, e.data2);
            break;
        }
        [[fallthrough]];
    case 0x8:
        adl_rt_noteOff(pl, channel, e.data1);
        break;
    case 0xb:
        adl_rt_controllerChange(pl, channel, e.data1, e.data2);
        break;
    case 0xc:
        adl_rt_patchChange(pl, channel, e.data1);
        break;
    case 0xe:
        adl_rt_pitchBendML(pl, channel, e.data2, e.data1);
        break;
    default:
        break;
    }
}

struct Options {
    std::string bank = ADLPLUG_CORECMP_DEFAULT_BANK;
    int reference = ADLMIDI_EMU_NUKED_OPL3_LLE;
    std::vector<int> cores = {ADLMIDI_EMU_ADLPLUG_OPL3, ADLMIDI_EMU_NUKED, ADLMIDI_EMU_NUKED_FAST, ADLMIDI_EMU_DOSBOX,
                              ADLMIDI_EMU_YMFM_OPL3};
    std::vector<std::string> scene_words;
    int chips = 1;
    unsigned jobs = 1;
    std::string csv;
    std::string dump;
    std::uint32_t warm_up = 8192;
    bool soft_pan = false;
};

struct Rendering {
    std::vector<std::int32_t> samples;  // interleaved: left, right
    double seconds = 0;                 // time spent generating
};

// Renders a scene with one core, as integers: the library writes a sample x of
// the chip as x / 32767 (adl_cvtReal), which this undoes.
std::optional<Rendering> render(const Options &opt, int emulator, const Scene &scene)
{
    struct Deleter {
        void operator()(ADL_MIDIPlayer *p) const noexcept { adl_close(p); }
    };
    const std::unique_ptr<ADL_MIDIPlayer, Deleter> player(adl_init(static_cast<long>(chip_rate)));
    ADL_MIDIPlayer *pl = player.get();
    if (pl == nullptr)
        throw std::runtime_error("adl_init failed");
    if (adl_switchEmulator(pl, emulator) != 0)
        return std::nullopt;
    if (adl_setNumChips(pl, opt.chips) != 0)
        throw std::runtime_error("adl_setNumChips failed");
    if (adl_openBankFile(pl, opt.bank.c_str()) != 0)
        throw std::runtime_error("cannot open the bank " + opt.bank + ": " + adl_errorInfo(pl));
    adl_setSoftPanEnabled(pl, opt.soft_pan ? 1 : 0);
    adl_reset(pl);

    Rendering out;
    out.samples.resize(2 * static_cast<std::size_t>(scene.frames));
    std::vector<double> left, right;
    for (std::uint32_t done = 0; done < opt.warm_up;) {
        const std::uint32_t count = std::min<std::uint32_t>(opt.warm_up - done, 4096);
        left.resize(count);
        right.resize(count);
        adl_generateDouble(pl, static_cast<int>(2 * count), left.data(), right.data(), 1);
        done += count;
    }
    std::size_t next = 0;
    std::uint32_t frame = 0;
    while (frame < scene.frames) {
        while (next < scene.events.size() && scene.events[next].frame <= frame)
            play(pl, scene.events[next++]);
        std::uint32_t until = scene.frames;
        if (next < scene.events.size())
            until = std::min(until, scene.events[next].frame);
        const std::uint32_t count = until - frame;
        left.resize(count);
        right.resize(count);
        const auto start = std::chrono::steady_clock::now();
        adl_generateDouble(pl, static_cast<int>(2 * count), left.data(), right.data(), 1);
        out.seconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        for (std::uint32_t i = 0; i < count; ++i) {
            const std::size_t at = 2 * (static_cast<std::size_t>(frame) + i);
            out.samples[at] = static_cast<std::int32_t>(std::lround(left[i] * 32767.0));
            out.samples[at + 1] = static_cast<std::int32_t>(std::lround(right[i] * 32767.0));
        }
        frame = until;
    }
    return out;
}

void dump(const Options &opt, const Scene &scene, int emulator, const Rendering &r)
{
    if (opt.dump.empty())
        return;
    const std::string path = opt.dump + "/" + scene.name + "." + std::to_string(emulator) + ".s32";
    std::FILE *f = std::fopen(path.c_str(), "wb");
    if (f == nullptr)
        throw std::runtime_error("cannot write " + path);
    const std::size_t written = std::fwrite(r.samples.data(), sizeof(std::int32_t), r.samples.size(), f);
    std::fclose(f);
    if (written != r.samples.size())
        throw std::runtime_error("cannot write " + path);
}

struct Comparison {
    double reference_energy = 0;
    double error_energy = 0;
    // With the core's output moved by the shift and scaled by the gain that
    // bring it closest to the reference: what is left is what the core makes
    // differently, apart from being late or early and louder or softer.
    double residual_energy = 0;
    double gain = 1;
    int shift = 0;
    std::int64_t largest = 0;
    std::size_t same = 0;
    std::size_t count = 0;
    double seconds = 0;
    double audio_seconds = 0;
};

struct Products {
    double cross = 0;  // of the core's output and the reference
    double own = 0;    // of the core's output with itself
};

// With the core's output moved by shift frames, x[f + shift] against ref[f],
// and nothing where it has no frame.
Products products(std::span<const std::int32_t> ref, std::span<const std::int32_t> x, int shift)
{
    const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(ref.size() / 2);
    Products p;
    for (std::ptrdiff_t f = std::max<std::ptrdiff_t>(0, -shift); f < std::min(n, n - shift); ++f) {
        const std::ptrdiff_t g = f + shift;
        for (std::ptrdiff_t ch = 0; ch < 2; ++ch) {
            const double r = static_cast<double>(ref[static_cast<std::size_t>(2 * f + ch)]);
            const double v = static_cast<double>(x[static_cast<std::size_t>(2 * g + ch)]);
            p.cross += v * r;
            p.own += v * v;
        }
    }
    return p;
}

Comparison compare(std::span<const std::int32_t> ref, std::span<const std::int32_t> x)
{
    Comparison c;
    c.count = ref.size();
    for (std::size_t i = 0; i < ref.size(); ++i) {
        const std::int64_t d = static_cast<std::int64_t>(x[i]) - ref[i];
        c.reference_energy += static_cast<double>(ref[i]) * static_cast<double>(ref[i]);
        c.error_energy += static_cast<double>(d) * static_cast<double>(d);
        c.largest = std::max(c.largest, d < 0 ? -d : d);
        if (d == 0)
            ++c.same;
    }
    // The best gain at a shift leaves sum((ref - gain x)^2) = reference energy
    // - cross^2 / own.
    c.residual_energy = c.reference_energy;
    c.gain = 0;
    for (int s = -max_shift; s <= max_shift; ++s) {
        const Products p = products(ref, x, s);
        if (p.own <= 0)
            continue;
        const double residual = std::max(0.0, c.reference_energy - p.cross * p.cross / p.own);
        if (residual < c.residual_energy) {
            c.residual_energy = residual;
            c.gain = p.cross / p.own;
            c.shift = s;
        }
    }
    return c;
}

std::string format_gain(double gain)
{
    if (gain <= 0)
        return "n/a";
    char text[32];
    std::snprintf(text, sizeof text, "%+.2f", 20.0 * std::log10(gain));
    return text;
}

double decibels(double reference, double error)
{
    if (error <= 0)
        return std::numeric_limits<double>::infinity();
    if (reference <= 0)
        return -std::numeric_limits<double>::infinity();
    return 10.0 * std::log10(reference / error);
}

std::string format_db(double db)
{
    if (std::isinf(db))
        return db > 0 ? "same" : "-inf";
    char text[32];
    std::snprintf(text, sizeof text, "%.1f", db);
    return text;
}

std::vector<std::string> split(std::string_view text)
{
    std::vector<std::string> parts;
    while (!text.empty()) {
        const std::size_t comma = text.find(',');
        parts.emplace_back(text.substr(0, comma));
        if (comma == std::string_view::npos)
            break;
        text.remove_prefix(comma + 1);
    }
    return parts;
}

Options parse(int argc, char **argv)
{
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        const auto value = [&]() -> std::string_view {
            if (i + 1 >= argc)
                throw std::runtime_error("missing a value after " + std::string(arg));
            return argv[++i];
        };
        if (arg == "--bank")
            opt.bank = value();
        else if (arg == "--reference")
            opt.reference = std::stoi(std::string(value()));
        else if (arg == "--cores") {
            opt.cores.clear();
            for (const std::string &part : split(value()))
                opt.cores.push_back(std::stoi(part));
        }
        else if (arg == "--scenes")
            opt.scene_words = split(value());
        else if (arg == "--chips")
            opt.chips = std::stoi(std::string(value()));
        else if (arg == "--jobs")
            opt.jobs = static_cast<unsigned>(std::max(1, std::stoi(std::string(value()))));
        else if (arg == "--csv")
            opt.csv = value();
        else if (arg == "--dump")
            opt.dump = value();
        else if (arg == "--warm-up")
            opt.warm_up = static_cast<std::uint32_t>(std::stoul(std::string(value())));
        else if (arg == "--soft-pan")
            opt.soft_pan = true;
        else
            throw std::runtime_error("unknown argument " + std::string(arg));
    }
    return opt;
}

const char *name_of(int emulator)
{
    switch (emulator) {
    case ADLMIDI_EMU_NUKED: return "Nuked OPL3 1.8";
    case ADLMIDI_EMU_NUKED_FAST: return "Nuked OPL3 Fast";
    case ADLMIDI_EMU_DOSBOX: return "DOSBox OPL3";
    case ADLMIDI_EMU_OPAL: return "Opal OPL3";
    case ADLMIDI_EMU_JAVA: return "Java OPL3";
    case ADLMIDI_EMU_ESFMu: return "ESFMu";
    case ADLMIDI_EMU_MAME_OPL2: return "MAME OPL2";
    case ADLMIDI_EMU_YMFM_OPL2: return "YMFM OPL2";
    case ADLMIDI_EMU_YMFM_OPL3: return "YMFM OPL3";
    case ADLMIDI_EMU_NUKED_OPL2_LLE: return "YM3812-LLE";
    case ADLMIDI_EMU_NUKED_OPL3_LLE: return "YMF262-LLE";
    case ADLMIDI_EMU_NUKED_OPL2_LITE: return "Nuked OPL2 Lite";
    case ADLMIDI_EMU_NUKED_CQM: return "Nuked CQM";
    case ADLMIDI_EMU_DOSBOX_OPL2: return "DOSBox OPL2";
    case ADLMIDI_EMU_ADLPLUG_OPL3: return "ADLplug-Next OPL3";
    default: return "?";
    }
}

}  // namespace

int main(int argc, char **argv)
{
    try {
        const Options opt = parse(argc, argv);
        std::vector<Scene> scenes = make_scenes();
        if (!opt.scene_words.empty()) {
            std::erase_if(scenes, [&](const Scene &s) {
                return std::none_of(opt.scene_words.begin(), opt.scene_words.end(),
                                    [&](const std::string &w) { return s.name.find(w) != std::string::npos; });
            });
        }
        if (scenes.empty())
            throw std::runtime_error("no scene matches --scenes");

        // results[core][scene], of the cores in this build
        std::vector<std::vector<Comparison>> results(opt.cores.size(), std::vector<Comparison>(scenes.size()));
        std::vector<double> reference_seconds(scenes.size());
        std::vector<std::atomic<bool>> core_missing(opt.cores.size());
        std::atomic<std::size_t> next{0};
        std::mutex progress;
        std::size_t finished = 0;
        auto worker = [&]() {
            for (std::size_t s = next++; s < scenes.size(); s = next++) {
                const std::optional<Rendering> ref = render(opt, opt.reference, scenes[s]);
                if (!ref)
                    throw std::runtime_error(std::string("the reference core is not in this build: ") + name_of(opt.reference));
                reference_seconds[s] = ref->seconds;
                dump(opt, scenes[s], opt.reference, *ref);
                for (std::size_t k = 0; k < opt.cores.size(); ++k) {
                    const std::optional<Rendering> x = render(opt, opt.cores[k], scenes[s]);
                    if (!x) {
                        core_missing[k] = true;
                        continue;
                    }
                    dump(opt, scenes[s], opt.cores[k], *x);
                    Comparison c = compare(ref->samples, x->samples);
                    c.seconds = x->seconds;
                    c.audio_seconds = static_cast<double>(scenes[s].frames) / static_cast<double>(chip_rate);
                    results[k][s] = c;
                }
                const std::lock_guard<std::mutex> lock(progress);
                ++finished;
                std::fprintf(stderr, "\r%zu of %zu scenes", finished, scenes.size());
            }
        };
        std::vector<std::thread> threads;
        threads.reserve(opt.jobs);
        std::exception_ptr failure;
        std::mutex failure_lock;
        for (unsigned j = 0; j < opt.jobs; ++j) {
            threads.emplace_back([&]() {
                try {
                    worker();
                }
                catch (...) {
                    const std::lock_guard<std::mutex> lock(failure_lock);
                    if (!failure)
                        failure = std::current_exception();
                    next = scenes.size();
                }
            });
        }
        for (std::thread &t : threads)
            t.join();
        std::fputc('\n', stderr);
        if (failure)
            std::rethrow_exception(failure);

        double audio = 0, reference_time = 0;
        for (std::size_t s = 0; s < scenes.size(); ++s) {
            audio += static_cast<double>(scenes[s].frames) / static_cast<double>(chip_rate);
            reference_time += reference_seconds[s];
        }
        std::printf("Reference: %s; bank %s; %zu scenes, %.1f s of audio at %u Hz, %d chip(s)\n",
                    name_of(opt.reference), opt.bank.c_str(), scenes.size(), audio, chip_rate, opt.chips);
        if (opt.jobs == 1)
            std::printf("%-18s speed %7.1fx\n", name_of(opt.reference), audio / reference_time);
        std::printf("%-18s %9s %9s %9s %8s %8s %6s %8s %11s %8s\n", "core", "error dB", "median", "worst",
                    "largest", "same %", "shift", "gain dB", "residual dB", "speed");
        for (std::size_t k = 0; k < opt.cores.size(); ++k) {
            if (core_missing[k]) {
                std::printf("%-18s not in this build\n", name_of(opt.cores[k]));
                continue;
            }
            double ref_e = 0, err_e = 0, residual_e = 0, seconds = 0;
            std::int64_t largest = 0;
            std::size_t same = 0, count = 0;
            std::vector<double> per_scene, gains;
            std::vector<int> shifts;
            double worst = std::numeric_limits<double>::infinity();
            for (std::size_t s = 0; s < scenes.size(); ++s) {
                const Comparison &c = results[k][s];
                ref_e += c.reference_energy;
                err_e += c.error_energy;
                residual_e += c.residual_energy;
                seconds += c.seconds;
                largest = std::max(largest, c.largest);
                same += c.same;
                count += c.count;
                const double db = decibels(c.reference_energy, c.error_energy);
                per_scene.push_back(db);
                worst = std::min(worst, db);
                shifts.push_back(c.shift);
                gains.push_back(c.gain);
            }
            std::sort(per_scene.begin(), per_scene.end());
            std::sort(shifts.begin(), shifts.end());
            std::sort(gains.begin(), gains.end());
            char speed[32] = "-";
            if (opt.jobs == 1)
                std::snprintf(speed, sizeof speed, "%.1fx", audio / seconds);
            std::printf("%-18s %9s %9s %9s %8lld %8.2f %6d %8s %11s %8s\n", name_of(opt.cores[k]),
                        format_db(decibels(ref_e, err_e)).c_str(), format_db(per_scene[per_scene.size() / 2]).c_str(),
                        format_db(worst).c_str(), static_cast<long long>(largest),
                        100.0 * static_cast<double>(same) / static_cast<double>(count), shifts[shifts.size() / 2],
                        format_gain(gains[gains.size() / 2]).c_str(), format_db(decibels(ref_e, residual_e)).c_str(),
                        speed);
        }
        std::printf("(median, shift and gain dB are medians over the scenes; the residual is what is left at each "
                    "scene's best shift and gain)\n");

        if (!opt.csv.empty()) {
            std::FILE *f = std::fopen(opt.csv.c_str(), "w");
            if (f == nullptr)
                throw std::runtime_error("cannot write " + opt.csv);
            std::fprintf(f, "core,scene,error_db,largest,same_percent,shift,gain_db,residual_db,speed\n");
            for (std::size_t k = 0; k < opt.cores.size(); ++k) {
                if (core_missing[k])
                    continue;
                for (std::size_t s = 0; s < scenes.size(); ++s) {
                    const Comparison &c = results[k][s];
                    std::fprintf(f, "%s,%s,%s,%lld,%.3f,%d,%s,%s,%.2f\n", name_of(opt.cores[k]),
                                 scenes[s].name.c_str(), format_db(decibels(c.reference_energy, c.error_energy)).c_str(),
                                 static_cast<long long>(c.largest),
                                 100.0 * static_cast<double>(c.same) / static_cast<double>(c.count), c.shift,
                                 format_gain(c.gain).c_str(),
                                 format_db(decibels(c.reference_energy, c.residual_energy)).c_str(),
                                 c.seconds > 0 ? c.audio_seconds / c.seconds : 0.0);
                }
            }
            std::fclose(f);
        }
        return 0;
    }
    catch (const std::exception &e) {
        std::fprintf(stderr, "ADLplug_corecmp: %s\n", e.what());
        return 1;
    }
}
