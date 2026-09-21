//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// SPDX-FileCopyrightText: 2018 Jean Pierre Cimalando
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

#include "ssgeg_waves.h"

double SSGEG_Waves::compute_wave(unsigned wave, double phase) const
{
    wave = wave & 7;

    const unsigned num_periods = num_periods_;

    if (num_periods == 0)
        return 0.0;

    phase = (phase < 0) ? 0 : phase;
    phase = (phase > 1) ? 1 : phase;

    phase *= num_periods;
    unsigned period = static_cast<unsigned>(phase);
    period = (period < num_periods) ? period : (num_periods - 1);
    phase -= period;

    const bool att = (wave & 4) != 0;
    const bool alt = (wave & 2) != 0;
    const bool hold = (wave & 1) != 0;

    if (period > 0 && hold)  // Hold
        return ((att ^ alt) != 0) ? 1.0 : -1.0;

    int dir = att ? +1 : -1;
    dir = (alt && (period & 1) != 0) ? -dir : dir;

    const double d = phase * 2 - 1;
    return (dir == +1) ? +d : -d;
}
