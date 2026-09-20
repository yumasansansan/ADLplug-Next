// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// Tests of the chip settings, which a project holds as properties: what the
// plugin writes comes back as it went in, and what an older project does not hold
// means what it meant before there was anything to hold.

#include "test.h"
#include "adl/chip_settings.h"
#include "JuceHeader.h"

ADLPLUG_TEST(chip_settings_round_trip)
{
    Chip_Settings cs;
    cs.emulator = 3;
    cs.chip_count = 7;
    cs.chan_alloc = 2;

    const Chip_Settings back = Chip_Settings::from_properties(cs.to_properties());
    CHECK(back.emulator == cs.emulator);
    CHECK(back.chip_count == cs.chip_count);
    CHECK(back.chan_alloc == cs.chan_alloc);
    CHECK(back == cs);
}

// A project saved before the way of taking a channel was a setting holds no such
// property, and the library's own choice is what it played with.
ADLPLUG_TEST(chip_settings_without_the_channel_allocation)
{
    PropertySet older;
    older.setValue("emulator", 1);
    older.setValue("chip_count", 2);

    const Chip_Settings cs = Chip_Settings::from_properties(older);
    CHECK(cs.emulator == 1u);
    CHECK(cs.chip_count == 2u);
    CHECK(cs.chan_alloc == -1);
}

// A number that is no mode is kept as the project wrote it. Holding it to the
// modes there are belongs to the step that gives a player its settings
// (playable_chip_settings), so that what a project says and what a chip is told
// stay two different things.
ADLPLUG_TEST(chip_settings_keep_a_channel_allocation_that_is_no_mode)
{
    Chip_Settings cs;
    cs.chan_alloc = 9;
    CHECK(Chip_Settings::from_properties(cs.to_properties()).chan_alloc == 9);
}
