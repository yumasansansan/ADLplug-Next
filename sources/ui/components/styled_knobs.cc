//          Copyright Jean Pierre Cimalando 2018.
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

#include "ui/components/styled_knobs.h"
#include "resources.h"

Styled_Knob_Skins::Styled_Knob_Skins()
    : knob_default(new Km_Skin)
    , slider_default(new Km_Skin)
{
    knob_default->load_data(Res::knob_skin.data, Res::knob_skin.size, 64);
    knob_default_small = knob_default->scaled(0.7);

    slider_default->style = Km_LinearHorizontal;
    slider_default->load_data(Res::slider_skin.data, Res::slider_skin.size, 64);
    slider_default_small = slider_default->scaled(0.5);
}
