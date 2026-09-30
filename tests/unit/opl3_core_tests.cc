// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// ADLplug-Next's own OPL3 core (sources/opl3/core): its ROMs as the functions
// they tabulate give them, its soft panning, its samples against those of the
// low-level core as libADLMIDI drives it, which they are to equal, and the
// icon the emulator menu shows for it.

#include "test.h"
#include "adl/chip_settings.h"
#include "opl3/core/core.h"
#include "opl3/core/ymf262_roms.h"
#include "JuceHeader.h"
#if defined(ADLPLUG_TESTS_HAVE_OPL3_LLE)
#include "chips/ymf262_lle/nopl3.h"
#endif
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <vector>

using adlplug::opl3::Core;

namespace {

struct Write {
    unsigned at, address, value;
};

// Random writes at random times, somewhat as a player makes them: notes on
// and off with random voices, pitch, level and panning changes, drums, the
// registers of the whole chip now and then, and any address at all but the
// test registers, which the core does not have.
std::vector<Write> random_writes(unsigned seed, unsigned samples)
{
    std::uint64_t state = 0x9e3779b97f4a7c15ULL * (seed + 1);
    const auto next = [&]() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return static_cast<unsigned>(state);
    };
    const auto below = [&](unsigned n) { return next() % n; };
    static constexpr unsigned op_offset[18] = {0, 1, 2, 3, 4, 5, 8, 9, 10, 11, 12, 13, 16, 17, 18, 19, 20, 21};
    std::vector<Write> w;
    const auto add = [&w](unsigned at, unsigned address, unsigned value) {
        w.push_back({.at = at, .address = address, .value = value});
    };
    add(0, 0x105, 1);
    add(0, 0x01, 0x20);
    std::array<unsigned, 18> fnum{}, block{};
    for (unsigned at = 0; at < samples;) {
        const unsigned kind = below(100);
        if (kind < 30) {
            const unsigned ch = below(18), c = ch % 9, bank = ch / 9;
            const unsigned mod = bank * 18 + (c / 3) * 6 + c % 3;
            for (const unsigned op : {mod, mod + 3}) {
                const unsigned off = (op >= 18 ? 0x100u : 0u) + op_offset[op % 18];
                add(at, 0x20 + off, next() & 0xff);
                add(at, 0x40 + off, below(3) == 0 ? next() & 0xff : below(40));
                add(at, 0x60 + off, next() & 0xff);
                add(at, 0x80 + off, next() & 0xff);
                add(at, 0xe0 + off, below(8));
            }
            const unsigned base = bank * 0x100 + c;
            add(at, 0xc0 + base, (next() & 0xff) | (below(4) == 0 ? 0u : 0x30u));
            fnum[ch] = below(1024);
            block[ch] = below(8);
            add(at, 0xa0 + base, fnum[ch] & 0xff);
            add(at, 0xb0 + base, 0x20 | (block[ch] << 2) | (fnum[ch] >> 8));
        }
        else if (kind < 50) {
            const unsigned ch = below(18);
            add(at, 0xb0 + (ch / 9) * 0x100 + ch % 9, (block[ch] << 2) | (fnum[ch] >> 8));
        }
        else if (kind < 60) {
            const unsigned ch = below(18), base = (ch / 9) * 0x100 + ch % 9;
            fnum[ch] = (fnum[ch] + below(64) + 1024 - 32) & 1023;
            add(at, 0xa0 + base, fnum[ch] & 0xff);
            add(at, 0xb0 + base, 0x20 | (block[ch] << 2) | (fnum[ch] >> 8));
        }
        else if (kind < 70) {
            const unsigned op = below(36);
            add(at, 0x40 + (op >= 18 ? 0x100u : 0u) + op_offset[op % 18], next() & 0xff);
        }
        else if (kind < 78) {
            const unsigned ch = below(18);
            add(at, 0xc0 + (ch / 9) * 0x100 + ch % 9, next() & 0xff);
        }
        else if (kind < 88)
            add(at, 0xbd, next() & 0xff);
        else if (kind < 91)
            add(at, 0x104, below(64));
        else if (kind < 92)
            add(at, 0x105, below(4) != 0 ? 1u : 0u);
        else if (kind < 94) {
            static constexpr unsigned regs[] = {0x08, 0x02, 0x03, 0x04, 0x102, 0x103, 0x108};
            const unsigned address = regs[below(7)];
            add(at, address, next() & 0xff);
        }
        else {
            unsigned address = below(0x200);
            if ((address & 0xff) == 0x01)
                address ^= 0x02;
            add(at, address, next() & 0xff);
        }
        at += below(4) == 0 ? 0 : below(3) == 0 ? below(400) : below(40);
    }
    return w;
}

// The core's samples for the writes, written before the sample they are at.
std::vector<std::int32_t> render(Core &core, const std::vector<Write> &writes, unsigned samples)
{
    std::vector<std::int32_t> out(2 * static_cast<std::size_t>(samples));
    std::size_t k = 0;
    for (unsigned n = 0; n < samples; ++n) {
        for (; k < writes.size() && writes[k].at <= n; ++k)
            core.write(static_cast<std::uint16_t>(writes[k].address), static_cast<std::uint8_t>(writes[k].value));
        core.generate(&out[2 * static_cast<std::size_t>(n)]);
    }
    return out;
}

}  // namespace

// The ROMs are what the functions they tabulate give (sources/opl3/core/ymf262_roms.h).
ADLPLUG_TEST(opl3_core_roms)
{
    const double pi = std::acos(-1.0);
    for (std::size_t i = 0; i < 256; ++i) {
        const double x = static_cast<double>(i);
        CHECK(adlplug::opl3::logsin_rom[i] == std::lround(-std::log2(std::sin((x + 0.5) * pi / 512.0)) * 256.0));
        CHECK(adlplug::opl3::exp_rom[i] == std::lround((std::exp2((255.0 - x) / 256.0) - 1.0) * 1024.0));
    }
}

// A core that nothing is written to makes nothing; one given writes makes
// sound.
ADLPLUG_TEST(opl3_core_silence_and_sound)
{
    Core quiet;
    std::int32_t frame[2];
    bool silent = true;
    for (int n = 0; n < 2000; ++n) {
        quiet.generate(frame);
        silent = silent && frame[0] == 0 && frame[1] == 0;
    }
    CHECK(silent);

    Core core;
    const std::vector<std::int32_t> out = render(core, random_writes(1, 4000), 4000);
    bool sound = false;
    for (const std::int32_t v : out)
        sound = sound || v != 0;
    CHECK(sound);
}

// Soft panning at the centre is the chip's own output, and panning hard to
// one side silences the other.
ADLPLUG_TEST(opl3_core_soft_panning)
{
    const std::vector<Write> writes = random_writes(2, 4000);
    Core plain, centred, left;
    for (unsigned ch = 0; ch < 18; ++ch) {
        const std::uint16_t address = static_cast<std::uint16_t>((ch / 9) * 0x100 + ch % 9);
        centred.write_pan(address, 64);
        left.write_pan(address, 0);
    }
    CHECK(render(plain, writes, 4000) == render(centred, writes, 4000));
    const std::vector<std::int32_t> l = render(left, writes, 4000);
    bool b_silent = true, a_heard = false;
    for (std::size_t n = 0; n < l.size(); n += 2) {
        a_heard = a_heard || l[n] != 0;
        b_silent = b_silent && l[n + 1] == 0;
    }
    CHECK(a_heard);
    CHECK(b_silent);
}

// The emulator menu shows the core with the icon of the plugin's executables
// (resources/application/ADLplug-96.png) rather than with a label of its name
// (Emulator_Icons, sources/opl3/adl/chip_settings.cc).
ADLPLUG_TEST(opl3_core_icon)
{
    // The other emulators' labels are text, and text wants JUCE's font
    // machinery set up and shut down (see text_icons).
    const ScopedJuceInitialiser_GUI juce_gui;
    const Emulator_Icons icons;
    const auto index = static_cast<std::size_t>(ADLMIDI_EMU_ADLPLUG_OPL3);
    CHECK(index < icons.images.size());
    if (index < icons.images.size()) {
        const Image &icon = icons.images[index];
        CHECK(icon.getWidth() == 96);
        CHECK(icon.getHeight() == 96);
    }
}

#if defined(ADLPLUG_TESTS_HAVE_OPL3_LLE)
// The core makes the samples the low-level core makes as libADLMIDI's wrapper
// drives it (nopl3.c), sample for sample, for random writes at random times.
ADLPLUG_TEST(opl3_core_matches_low_level_core)
{
    constexpr unsigned samples = 6000;
    for (unsigned seed = 1; seed <= 3; ++seed) {
        const std::vector<Write> writes = random_writes(seed, samples);
        Core core;
        const std::vector<std::int32_t> ours = render(core, writes, samples);
        void *chip = nopl3_init(14318182, 49716);
        std::size_t k = 0, differ = 0;
        for (unsigned n = 0; n < samples; ++n) {
            for (; k < writes.size() && writes[k].at <= n; ++k)
                nopl3_write_buf(chip, static_cast<unsigned short>(writes[k].address), static_cast<unsigned char>(writes[k].value));
            short frame[2];
            nopl3_getsample_one_native(chip, frame);
            const std::size_t i = 2 * static_cast<std::size_t>(n);
            if (ours[i] != frame[0] || ours[i + 1] != frame[1]) {
                if (differ == 0)
                    std::fprintf(stderr, "  seed %u: sample %u is %d %d, the low-level core's %d %d\n", seed, n,
                                 static_cast<int>(ours[i]), static_cast<int>(ours[i + 1]), frame[0], frame[1]);
                ++differ;
            }
        }
        nopl3_shutdown(chip);
        CHECK(differ == 0);
    }
}
#endif
