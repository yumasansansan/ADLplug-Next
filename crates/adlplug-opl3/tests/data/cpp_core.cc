// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// The program that wrote cpp_core.txt: the C++ version's OPL3 core, given the
// random writes of the C++ version's tests (random_writes below is theirs, as
// tests/unit/opl3_core_tests.cc has it), and the low-level core YMF262-LLE given
// the same, to show that the C++ core's samples are the chip's where soft panning
// plays no part. It is kept for where the reference came from and to make it
// again; it builds against the C++ version's tree, which this branch no longer
// holds -- commit b4c4d0c, with its submodules checked out and their patches
// applied, as CMake's configure applies them. From that tree:
//
//     L=thirdparty/libADLMIDI/src/chips/ymf262_lle
//     clang -std=c23 -O2 -c $L/nopl3.c $L/nuked_fmopl3.c
//     clang++ -std=c++23 -O2 -I sources/opl3/core -I $L cpp_core.cc \
//         sources/opl3/core/core.cc nopl3.o nuked_fmopl3.o -o cpp_core
//     ./cpp_core > cpp_core.txt   # then put the header of cpp_core.txt back
//
// It exits with 1 when the low-level core differs from the C++ core anywhere.

#include "core.h"
#include "nopl3.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
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


constexpr std::uint64_t basis = 14695981039346656037ull;
std::uint64_t fnv(std::uint64_t h, const void *p, std::size_t n)
{
    const auto *b = static_cast<const unsigned char *>(p);
    for (std::size_t i = 0; i < n; ++i) {
        h ^= b[i];
        h *= 1099511628211ull;
    }
    return h;
}
}  // namespace

int main()
{
    constexpr unsigned samples = 20000, block = 1000;
    long lle_mismatches = 0;
    for (unsigned seed = 1; seed <= 8; ++seed) {
        const std::vector<Write> writes = random_writes(seed, samples);
        std::uint64_t h = basis;
        for (const Write &w : writes) {
            const std::uint32_t at = w.at;
            const std::uint16_t address = static_cast<std::uint16_t>(w.address);
            const std::uint8_t value = static_cast<std::uint8_t>(w.value);
            h = fnv(h, &at, 4);
            h = fnv(h, &address, 2);
            h = fnv(h, &value, 1);
        }
        std::printf("writes %u %zu %016llx\n", seed, writes.size(), static_cast<unsigned long long>(h));
        for (const bool panned : {false, true}) {
            Core core(Core::Pass::cpp);
            void *chip = panned ? nullptr : nopl3_init(14318182, 49716);
            std::printf("%s %u", panned ? "panned" : "plain", seed);
            std::size_t k = 0;
            std::uint64_t bh = basis;
            for (unsigned n = 0; n < samples; ++n) {
                for (; k < writes.size() && writes[k].at <= n; ++k) {
                    core.write(static_cast<std::uint16_t>(writes[k].address), static_cast<std::uint8_t>(writes[k].value));
                    if (chip != nullptr)
                        nopl3_write_buf(chip, static_cast<unsigned short>(writes[k].address), static_cast<unsigned char>(writes[k].value));
                }
                if (panned && n % 500 == 0) {
                    const unsigned ch = (n / 500 * 7 + seed) % 18;
                    const auto address = static_cast<std::uint16_t>((ch / 9) * 0x100 + ch % 9);
                    const auto pan = static_cast<std::uint8_t>(n % 1500 == 0 ? 64 : (n / 500 * 37 + seed * 11) % 128);
                    core.write_pan(address, pan);
                }
                std::int32_t frame[2];
                core.generate(frame);
                bh = fnv(bh, frame, sizeof frame);
                if (chip != nullptr) {
                    short lle[2];
                    nopl3_getsample_one_native(chip, lle);
                    if (lle[0] != frame[0] || lle[1] != frame[1])
                        ++lle_mismatches;
                }
                if ((n + 1) % block == 0) {
                    std::printf(" %016llx", static_cast<unsigned long long>(bh));
                    bh = basis;
                }
            }
            std::printf("\n");
            if (chip != nullptr)
                nopl3_shutdown(chip);
        }
    }
    std::fprintf(stderr, "samples where the low-level core differs: %ld\n", lle_mismatches);
    return lle_mismatches == 0 ? 0 : 1;
}
