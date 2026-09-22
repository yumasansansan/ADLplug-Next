//          Copyright Jean Pierre Cimalando 2019.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// SPDX-FileCopyrightText: 2019 Jean Pierre Cimalando
// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: BSL-1.0 AND GPL-3.0-or-later
//
// This file comes from ADLplug and was modified for ADLplug-Next. The notice at
// the top is ADLplug's; the LICENSE it names was ADLplug's copy of the Boost
// Software License, now LICENSES/BSL-1.0.txt. The SPDX lines name the copyright
// holders and licenses in the machine-readable form of the REUSE specification:
// ADLplug's code is under the Boost Software License 1.0, and ADLplug-Next's
// changes are under the GNU General Public License, version 3 or any later
// version (LICENSES/GPL-3.0-or-later.txt).

#pragma once
class Player;
struct Chip_Settings;
struct Instrument_Global_Parameters;

// Bits for the parts of the state that have changed and wait to be sent.
enum State_Change_Bit : unsigned
{
    Cb_ChipSettings,
    Cb_GlobalParameters,
    Cb_ActivePart,
    Cb_BankTitle,
    Cb_Resampling,
    Cb_Instrument1,
    Cb_Instrument16 = Cb_Instrument1 + 15,
    Cb_Selection1,
    Cb_Selection16 = Cb_Selection1 + 15,
    Cb_Count,
};

Chip_Settings get_player_chip_settings(const Player &pl);
Instrument_Global_Parameters get_player_global_parameters(const Player &pl);
// Gives the player playable_chip_settings(cs).
void set_player_chip_settings(Player &pl, const Chip_Settings &cs);
void set_player_global_parameters(Player &pl, const Instrument_Global_Parameters &gp);

// The chip settings the player runs for those of the parameters or of a saved
// state, which keep what they were given: an emulator this build has (see
// available_emulator()), 1 to 100 chips, and on OPL3 no more four-operator
// channels than the chips have, six each.
Chip_Settings playable_chip_settings(const Chip_Settings &cs);
