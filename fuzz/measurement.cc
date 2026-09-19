// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// How long an instrument sounds. The plugin measures that for every instrument
// it is given -- from a bank file, from a project, or from the knobs of the
// editor -- because a host wants to know when a note has finished. The worker
// does it on a thread of its own with Measurer::ComputeDurations, which plays
// the instrument on a chip of its own for as long as forty seconds and then
// listens to the silence for sixty more, and turns what it hears into
// milliseconds.
//
// The input is the instrument, as the registers of the chip hold it: the bytes
// go into the library's own structure, which is what a bank file fills and what
// the editor writes into. Bytes the input does not reach are zero.
//
// What is checked: the sanitizers, that every number the measurement gives back
// is finite, and that it does not claim more time than it simulated. The worker
// narrows the two times to sixteen bits for the message it sends, and the editor
// shows them; a measurement that came back with a hundred seconds, or with the
// milliseconds of a time that was never there, would put a note's end in the
// wrong place for the host.
//
// One input is heavy: a hundred seconds of audio, at 49716 Hz on OPL3 and
// 53267 on OPN2, for the one instrument, and the sanitizers make that slower
// still. The target is left out of the fuzzing that every push does
// (ci/fuzz.sh runs it only when it is told to include the long ones) and runs
// in the daily fuzzing instead. The replay test runs on every system as the
// others do, since replaying a handful of inputs is quick.

#include "fuzz.h"
#include "adl/instrument.h"
#include "adl/measurer.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

// The structure the library keeps an instrument in, which a bank file fills.
#if defined(ADLPLUG_OPL3)
using Native_Instrument = ADL_Instrument;
#elif defined(ADLPLUG_OPN2)
using Native_Instrument = OPN2_Instrument;
#endif

// What the measurement can reach: it plays for forty seconds at the most and
// listens for sixty (Measurer::ComputeDurations), in milliseconds.
constexpr std::uint64_t sounding_max = 40 * 1000;
constexpr std::uint64_t silence_max = 60 * 1000;

}  // namespace

int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    Native_Instrument native {};
    std::memcpy(&native, data, std::min(size, sizeof native));

    const Instrument instrument = Instrument::from_adlmidi(native);

    Measurer::DurationInfo measured {};
    Measurer::ComputeDurations(instrument, measured);

    FUZZ_CHECK(std::isfinite(measured.peak_amplitude_value));
    FUZZ_CHECK(std::isfinite(measured.quarter_amplitude_time));
    FUZZ_CHECK(std::isfinite(measured.begin_amplitude));
    FUZZ_CHECK(std::isfinite(measured.interval));
    FUZZ_CHECK(std::isfinite(measured.keyoff_out_time));
    FUZZ_CHECK(measured.ms_sound_kon <= sounding_max);
    FUZZ_CHECK(measured.ms_sound_koff <= silence_max);

    return 0;
}
