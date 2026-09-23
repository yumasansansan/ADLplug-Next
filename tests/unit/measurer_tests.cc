// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// Tests of the measurer, which the worker runs to learn how long an instrument
// sounds.

#include "test.h"
#include "adl/instrument.h"
#include "adl/measurer.h"
#include "adl/wopx_file.h"
#include "resources.h"
#include "utility/pak.h"
#include "JuceHeader.h"
#include <atomic>
#include <cstdint>
#include <vector>

namespace {

// The first instrument of the first melodic bank the plugin carries, which is
// something that sounds.
bool first_instrument(Instrument &ins)
{
    Pak_File_Reader pak;
    CHECK(pak.init_with_data(Res::banks_pak.data, Res::banks_pak.size));
    std::vector<std::uint8_t> data = pak.extract(0);
    const WOPx::BankFile_Ptr file(WOPx::LoadBankFromMem(data.data(), data.size(), nullptr));
    CHECK(file != nullptr);
    if (file == nullptr)
        return false;

    std::vector<Midi_Bank> banks;
    Instrument_Global_Parameters igp;
    Midi_Bank::from_wopl(*file, banks, igp);
    CHECK(!banks.empty());
    if (banks.empty())
        return false;

    ins = banks.at(0).ins[0];
    return true;
}

}  // namespace

// A measurement told to stop before it has begun gives up at once and says so.
// That is what lets the worker stop, and a host that is waiting for it go on,
// without the instrument being played for the rest of up to a hundred seconds.
ADLPLUG_TEST(measurement_gives_up_when_told)
{
    Instrument ins;
    if (!first_instrument(ins))
        return;

    const std::atomic<bool> stop {true};
    Measurer::DurationInfo result {};
    CHECK(!Measurer::ComputeDurations(ins, result, &stop));
}

// Given no stop, or one that is never set, the measurement runs to its end, and
// looking at the stop changes nothing it finds.
ADLPLUG_TEST(measurement_runs_to_its_end_unless_told)
{
    Instrument ins;
    if (!first_instrument(ins))
        return;

    Measurer::DurationInfo plain {};
    CHECK(Measurer::ComputeDurations(ins, plain));
    CHECK(!plain.nosound);

    const std::atomic<bool> stop {false};
    Measurer::DurationInfo watched {};
    CHECK(Measurer::ComputeDurations(ins, watched, &stop));
    CHECK(watched.ms_sound_kon == plain.ms_sound_kon);
    CHECK(watched.ms_sound_koff == plain.ms_sound_koff);
    CHECK(watched.peak_amplitude_time == plain.peak_amplitude_time);
    CHECK(watched.peak_amplitude_value == plain.peak_amplitude_value);
    CHECK(watched.nosound == plain.nosound);
}
