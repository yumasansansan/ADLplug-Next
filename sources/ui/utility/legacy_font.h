//          Part of ADLplug, distributed under the GNU GPL v3.
//               (See accompanying file LICENSE.)
//
// ADLplug's layouts were drawn against JUCE 5/6 font metrics. JUCE 8 moved its
// default to "portable" metrics, under which the same nominal height may render
// at a different size -- a visible change in the fixed-size labels the old
// Projucer GUI editor laid out. These helpers pin the old ("legacy") metrics so
// the UI keeps its original proportions. Custom_Look_And_Feel does the same for
// the fonts JUCE widgets choose on their own.

#pragma once
#include "JuceHeader.h"

inline FontOptions legacy_font(float height, int style_flags = Font::plain)
{
    return FontOptions(height, style_flags).withMetricsKind(TypefaceMetricsKind::legacy);
}

inline FontOptions legacy_font(const String &typeface_name, float height, int style_flags = Font::plain)
{
    return FontOptions(typeface_name, height, style_flags).withMetricsKind(TypefaceMetricsKind::legacy);
}
