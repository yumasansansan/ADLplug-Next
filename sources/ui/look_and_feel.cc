//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

#include "ui/look_and_feel.h"
#include "resources.h"
#include <algorithm>

#if 1
#   define trace(fmt, ...)
#else
#   define trace(fmt, ...) fprintf(stderr, "[LF] " fmt "\n" __VA_OPT__(,) __VA_ARGS__)
#endif

//==============================================================================
void Custom_Look_And_Feel::add_custom_tooltip(const String &key, Component *component, bool owned)
{
    custom_tooltips_[key].component = OptionalScopedPointer<Component>(component, owned);
}

//==============================================================================
Typeface::Ptr Custom_Look_And_Feel::getTypefaceForFont(const Font &font)
{
    const String &name = font.getTypefaceName();
    const String &style = font.getTypefaceStyle();

    trace("Query Typeface name='%s' style='%s'",
          name.toRawUTF8(), style.toRawUTF8());

    Typeface::Ptr tf;
    if (name == Font::getDefaultSansSerifFontName()) {
        if (style == "Italic")
            tf = getOrCreateFont(fontSansItalic, Res::Sans_Italic);
        else if (style == "Bold")
            tf = getOrCreateFont(fontSansBold, Res::Sans_Bold);
        else if (style == "Bold Italic")
            tf = getOrCreateFont(fontSansBoldItalic, Res::Sans_BoldItalic);
        else
            tf = getOrCreateFont(fontSansRegular, Res::Sans_Regular);
    }
    else if (name == Font::getDefaultSerifFontName()) {
        if (style == "Italic")
            tf = getOrCreateFont(fontSerifItalic, Res::Serif_Italic);
        else if (style == "Bold")
            tf = getOrCreateFont(fontSerifBold, Res::Serif_Bold);
        else if (style == "Bold Italic")
            tf = getOrCreateFont(fontSerifBoldItalic, Res::Serif_BoldItalic);
        else
            tf = getOrCreateFont(fontSerifRegular, Res::Serif_Regular);
    }
    else if (name == Font::getDefaultMonospacedFontName()) {
        if (style == "Italic")
            tf = getOrCreateFont(fontMonoItalic, Res::Mono_Italic);
        else if (style == "Bold")
            tf = getOrCreateFont(fontMonoBold, Res::Mono_Bold);
        else if (style == "Bold Italic")
            tf = getOrCreateFont(fontMonoBoldItalic, Res::Mono_BoldItalic);
        else
            tf = getOrCreateFont(fontMonoRegular, Res::Mono_Regular);
    }

    if (!tf) {
        trace("Typeface not found, fallback");
        tf = LookAndFeel::getTypefaceForFont(font);
    }

    return tf;
}

Typeface::Ptr Custom_Look_And_Feel::getOrCreateFont(Typeface::Ptr &font, const Res_Data &data)
{
    if (!font) {
        MemoryInputStream mem_stream(data.data, data.size, false);
        GZIPDecompressorInputStream gz_stream(&mem_stream, false, GZIPDecompressorInputStream::gzipFormat);

        MemoryBlock mem_block;
        gz_stream.readIntoMemoryBlock(mem_block);

        font = Typeface::createSystemTypefaceFor(mem_block.getData(), mem_block.getSize());

        if (!font)
            trace("Could not load font data.");
        else
            trace("Font loaded name='%s' style='%s'",
                  font->getName().toRawUTF8(), font->getStyle().toRawUTF8());
    }
    return font;
}

void Custom_Look_And_Feel::drawButtonBackground(Graphics &g, Button &button, const Colour &background_colour, bool is_mouse_over_button, bool is_button_down)
{
    const float corner_size = 6.0f;
    const Rectangle<float> bounds = button.getLocalBounds().toFloat().reduced(0.5f, 0.5f);

    auto base_colour = background_colour
        .withMultipliedSaturation(button.hasKeyboardFocus(true) ? 1.3f : 0.9f)
        .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f);
    if (is_button_down || is_mouse_over_button)
        base_colour = base_colour.contrasting(is_button_down ? 0.2f : 0.05f);

    g.setColour(base_colour);
    if (button.isConnectedOnLeft() || button.isConnectedOnRight() ||
        button.isConnectedOnTop() || button.isConnectedOnBottom()) {
        Path path;
        path.addRoundedRectangle(bounds.getX(), bounds.getY(),
                                 bounds.getWidth(), bounds.getHeight(),
                                 corner_size, corner_size,
                                 !button.isConnectedOnLeft() && !button.isConnectedOnTop(),
                                 !button.isConnectedOnRight() && !button.isConnectedOnTop(),
                                 !button.isConnectedOnLeft() && !button.isConnectedOnBottom(),
                                 !button.isConnectedOnRight() && !button.isConnectedOnBottom());
        g.fillPath(path);
        g.setColour(button.findColour(ComboBox::outlineColourId));
        g.strokePath(path, PathStrokeType(1.0f));
    }
    else {
        g.fillRoundedRectangle(bounds, corner_size);
        g.setColour(button.findColour(ComboBox::outlineColourId));
        g.drawRoundedRectangle(bounds, corner_size, 1.0f);
    }
}

Font Custom_Look_And_Feel::getComboBoxFont(ComboBox &box)
{
    return withDefaultMetrics(FontOptions(static_cast<float>(std::min(15.0, box.getHeight() * 0.85))));
}

Rectangle<int> Custom_Look_And_Feel::getTooltipBounds(const String &text, Point<int> pos, Rectangle<int> parent_area)
{
    if (text.startsWith("<<") && text.endsWith(">>")) {
        const String key = text.substring(2, text.length() - 2);
        const auto it = custom_tooltips_.find(key);
        if (it != custom_tooltips_.end()) {
            const Component *comp = it->second.component.get();
            const int w = comp->getWidth() + 14;
            const int h = comp->getHeight() + 6;
            return Rectangle<int>(pos.x > parent_area.getCentreX() ? pos.x - (w + 12) : pos.x + 24,
                                  pos.y > parent_area.getCentreY() ? pos.y - (h + 6) : pos.y + 6,
                                  w, h).constrainedWithin(parent_area);
        }
    }
    return Base::getTooltipBounds(text, pos, parent_area);
}

void Custom_Look_And_Feel::drawTooltip(Graphics &g, const String &text, int width, int height)
{
    if (text.startsWith("<<") && text.endsWith(">>")) {
        const String key = text.substring(2, text.length() - 2);
        const auto it = custom_tooltips_.find(key);
        if (it != custom_tooltips_.end()) {
            Component *comp = it->second.component.get();
            const Rectangle<int> bounds(width, height);
            const float corner_size = 5.0f;
            g.setColour(findColour(TooltipWindow::backgroundColourId));
            g.fillRoundedRectangle(bounds.toFloat(), corner_size);
            g.setColour(findColour(TooltipWindow::outlineColourId));
            g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f, 0.5f), corner_size, 1.0f);
            g.setOrigin((width - comp->getWidth()) / 2, (height - comp->getHeight()) / 2);
            comp->paintEntireComponent(g, false);
            return;
        }
    }
    Base::drawTooltip(g, text, width, height);
}
