//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

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
