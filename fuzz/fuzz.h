// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// What every fuzz target shares (plan D48). A target defines
// LLVMFuzzerTestOneInput, which libFuzzer calls with each input it makes up
// (ADLplug_BUILD_FUZZERS), and which replay.cc calls with the files it is given
// on every system (ADLplug_BUILD_TESTS). The sanitizers catch what goes wrong
// in memory; FUZZ_CHECK stops the program where a target finds that something
// it expects of the input does not hold, so that libFuzzer keeps the input.

#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

#define FUZZ_CHECK(condition)                                               \
    do {                                                                    \
        if (!(condition)) {                                                 \
            std::fprintf(stderr, "%s:%d: FUZZ_CHECK(%s) failed\n",          \
                         __FILE__, __LINE__, #condition);                   \
            std::abort();                                                   \
        }                                                                   \
    } while (false)
