//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

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
