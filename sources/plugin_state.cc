//          Copyright Jean Pierre Cimalando 2019.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// SPDX-FileCopyrightText: 2018-2019 Jean Pierre Cimalando
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

#include "plugin_state.h"
#include "parameter_block.h"
#include "adl/chip_settings.h"
#include "adl/player.h"
#include <algorithm>

// The modes of channel allocation that the library knows, which are numbered from
// nought, with its own choice below them at -1.
#if defined(ADLPLUG_OPL3)
constexpr int chan_alloc_modes = ADLMIDI_ChanAlloc_Count;
#elif defined(ADLPLUG_OPN2)
constexpr int chan_alloc_modes = OPNMIDI_ChanAlloc_Count;
#endif

Chip_Settings get_player_chip_settings(const Player &pl)
{
    Chip_Settings cs;
    cs.emulator = pl.emulator();
    cs.chip_count = pl.num_chips();
    cs.chan_alloc = pl.channel_alloc_mode();
#if defined(ADLPLUG_OPL3)
    cs.fourop_count = pl.num_4ops();
#elif defined(ADLPLUG_OPN2)
    cs.chip_type = pl.chip_type();
#endif
    return cs;
}

// The player numbers volume models from 1, with 0 meaning automatic; the
// parameter and the WOPL/WOPN files number them from 0.
Instrument_Global_Parameters get_player_global_parameters(const Player &pl)
{
    Instrument_Global_Parameters gp;
    gp.volume_model = pl.volume_model() - 1;
#if defined(ADLPLUG_OPL3)
    gp.deep_tremolo = pl.deep_tremolo();
    gp.deep_vibrato = pl.deep_vibrato();
    gp.mt32_defaults = pl.mt32_defaults();
#elif defined(ADLPLUG_OPN2)
    gp.lfo_enable = pl.lfo_enabled();
    gp.lfo_frequency = pl.lfo_frequency();
#endif
    return gp;
}

Chip_Settings playable_chip_settings(const Chip_Settings &cs)
{
    Chip_Settings playable = cs;
    playable.emulator = available_emulator(cs.emulator);
    // A hundred chips is what the libraries take (ADL_MAX_CHIPS, OPN_MAX_CHIPS,
    // which their private headers hold), and what the plugin's own parameter
    // offers; a project can hold any number, and more than they take would have
    // the library refuse the lot and keep what it had.
    playable.chip_count = std::clamp(cs.chip_count, 1u, 100u);
    // The modes the library knows, and its own choice below them: a project can
    // hold any number, and one that is no mode would leave the library choosing for
    // itself without anything saying so.
    playable.chan_alloc = std::clamp(cs.chan_alloc, -1, chan_alloc_modes - 1);
#if defined(ADLPLUG_OPL3)
    playable.fourop_count = std::min(cs.fourop_count, 6 * playable.chip_count);
#elif defined(ADLPLUG_OPN2)
    // The chips the library has, the last of them being the highest number it
    // knows: a project can hold any number, and a number that is no chip would
    // have the library make a family of it that is none.
    playable.chip_type = std::min(cs.chip_type, static_cast<unsigned>(OPNMIDI_ChipType_OPNA));
#endif
    return playable;
}

void set_player_chip_settings(Player &pl, const Chip_Settings &cs)
{
    const Chip_Settings playable = playable_chip_settings(cs);
    pl.set_emulator(playable.emulator);
    pl.set_num_chips(playable.chip_count);
    pl.set_channel_alloc_mode(playable.chan_alloc);
#if defined(ADLPLUG_OPL3)
    pl.set_num_4ops(playable.fourop_count);
#elif defined(ADLPLUG_OPN2)
    pl.set_chip_type(playable.chip_type);
#endif
}

void set_player_global_parameters(Player &pl, const Instrument_Global_Parameters &gp)
{
    pl.set_volume_model(gp.volume_model + 1);
#if defined(ADLPLUG_OPL3)
    pl.set_deep_tremolo(gp.deep_tremolo);
    pl.set_deep_vibrato(gp.deep_vibrato);
    pl.set_mt32_defaults(gp.mt32_defaults);
#elif defined(ADLPLUG_OPN2)
    pl.set_lfo_enabled(gp.lfo_enable);
    pl.set_lfo_frequency(gp.lfo_frequency);
#endif
}
