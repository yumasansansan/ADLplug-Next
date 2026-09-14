//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// SPDX-FileCopyrightText: 2018, 2021 Jean Pierre Cimalando
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
#include "JuceHeader.h"
#include <unordered_map>
class Custom_Tooltips;
struct Res_Data;

class Custom_Look_And_Feel : public LookAndFeel_V4
{
public:
    using Base = LookAndFeel_V4;

    //==========================================================================
    void add_custom_tooltip(const String &key, Component *component, bool owned);

    //==========================================================================
    Typeface::Ptr getTypefaceForFont(const Font &font) override;

    void drawButtonBackground(Graphics &g, Button &button, const Colour &background_colour, bool is_mouse_over_button, bool is_button_down) override;

    Font getComboBoxFont(ComboBox &box) override;

    Rectangle<int> getTooltipBounds(const String &text, Point<int> pos, Rectangle<int> parent_area) override;
    void drawTooltip(Graphics &g, const String &text, int width, int height) override;

private:
    struct Custom_Tooltip_Entry {
        OptionalScopedPointer<Component> component;
    };
    std::unordered_map<String, Custom_Tooltip_Entry> custom_tooltips_;

    static Typeface::Ptr getOrCreateFont(Typeface::Ptr &font, const Res_Data &data);

    Typeface::Ptr fontSansRegular;
    Typeface::Ptr fontSansItalic;
    Typeface::Ptr fontSansBold;
    Typeface::Ptr fontSansBoldItalic;
    Typeface::Ptr fontSerifRegular;
    Typeface::Ptr fontSerifItalic;
    Typeface::Ptr fontSerifBold;
    Typeface::Ptr fontSerifBoldItalic;
    Typeface::Ptr fontMonoRegular;
    Typeface::Ptr fontMonoItalic;
    Typeface::Ptr fontMonoBold;
    Typeface::Ptr fontMonoBoldItalic;
};
