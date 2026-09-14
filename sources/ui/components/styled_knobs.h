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

#pragma once
#include "ui/components/knob_component.h"
#include "ui/utility/knobman_skin.h"

// The skins of the styled knobs. Each knob holds them through a
// SharedResourcePointer, so they are loaded with the first knob and released
// with the last one, instead of staying in static storage until the module is
// unloaded (see Emulator_Icons).
struct Styled_Knob_Skins {
    Styled_Knob_Skins();
    Km_Skin_Ptr knob_default;
    Km_Skin_Ptr knob_default_small;
    Km_Skin_Ptr slider_default;
    Km_Skin_Ptr slider_default_small;
};

class Styled_Knob_Default : public Knob
{
public:
    Styled_Knob_Default() { set_skin(skins_->knob_default.get()); }
private:
    SharedResourcePointer<Styled_Knob_Skins> skins_;
};

//------------------------------------------------------------------------------
class Styled_Knob_DefaultSmall : public Knob
{
public:
    Styled_Knob_DefaultSmall() { set_skin(skins_->knob_default_small.get()); }
private:
    SharedResourcePointer<Styled_Knob_Skins> skins_;
};

//------------------------------------------------------------------------------
class Styled_Slider_Default : public Knob
{
public:
    Styled_Slider_Default() { set_skin(skins_->slider_default.get()); }
private:
    SharedResourcePointer<Styled_Knob_Skins> skins_;
};

//------------------------------------------------------------------------------
class Styled_Slider_DefaultSmall : public Knob
{
public:
    Styled_Slider_DefaultSmall() { set_skin(skins_->slider_default_small.get()); }
private:
    SharedResourcePointer<Styled_Knob_Skins> skins_;
};
