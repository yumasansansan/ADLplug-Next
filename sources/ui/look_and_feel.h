//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

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
