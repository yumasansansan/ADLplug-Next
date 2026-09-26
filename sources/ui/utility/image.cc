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

#include "ui/utility/image.h"
#include <algorithm>
#include <cmath>

namespace Image_Utils {

namespace {

// Twice the height the editor shows the emulator icons at, so the label stays
// sharp on high-density displays; opaque, so it reads on any background.
constexpr int icon_height = 40;
constexpr float icon_padding = 10.0f;
constexpr float icon_corner = 6.0f;

Font icon_font()
{
    return Font(FontOptions(26.0f, Font::bold));
}

int icon_width(const Font &font, const String &text)
{
    const auto width = static_cast<int>(
        std::ceil(GlyphArrangement::getStringWidth(font, text) + 2.0f * icon_padding));
    return std::max(width, icon_height);
}

// The label of `text` in `bounds`, which are those of the whole label.
void draw_text_icon(Graphics &g, const Font &font, const String &text, Rectangle<int> bounds)
{
    const Rectangle<float> frame = bounds.toFloat().reduced(1.0f);
    g.setColour(Colour(0xff4f5d64));
    g.fillRoundedRectangle(frame, icon_corner);
    g.setColour(Colour(0xffa3b3ba));
    g.drawRoundedRectangle(frame, icon_corner, 2.0f);
    g.setColour(Colours::white);
    g.setFont(font);
    g.drawText(text, bounds, Justification::centred, false);
}

}  // namespace

Image make_text_icon(const String &text, const ImageType &type)
{
    const Font font = icon_font();
    const Image image(type.create(Image::ARGB, icon_width(font, text), icon_height, true));
    {
        Graphics g(image);
        draw_text_icon(g, font, text, image.getBounds());
    }
    return image;
}

std::vector<Image> make_text_icons(const StringArray &texts, const ImageType &type)
{
    // One row of the image a label, and a gap of two pixels between the rows,
    // as the cells of the knobs' small skins are laid out. Direct2D does not
    // draw a label below the first row quite as it draws one at the corner of
    // a target: measured, up to 112 of a label's 3560 pixels differ from
    // make_text_icon's, by one or two levels. The software renderer draws them
    // alike (tests/unit/utility_tests.cc).
    std::vector<Image> icons;
    if (texts.isEmpty())
        return icons;

    const Font font = icon_font();
    std::vector<int> widths;
    int widest = 0;
    for (const String &text : texts) {
        widths.push_back(icon_width(font, text));
        widest = std::max(widest, widths.back());
    }
    constexpr int gap = 2;
    constexpr int pitch = icon_height + gap;
    const Image atlas(type.create(Image::ARGB, widest, pitch * texts.size() - gap, true));
    {
        Graphics g(atlas);
        for (int i = 0; i < texts.size(); ++i)
            draw_text_icon(g, font, texts[i],
                           {0, i * pitch, widths[static_cast<std::size_t>(i)], icon_height});
    }

    icons.reserve(widths.size());
    for (int i = 0; i < texts.size(); ++i)
        icons.push_back(
            atlas.getClippedImage({0, i * pitch, widths[static_cast<std::size_t>(i)], icon_height}));
    return icons;
}

Rectangle<int> get_image_solid_area(const Image &img)
{
    Rectangle<int> bounds = img.getBounds();

    const auto col_transparent =
        [&](int c) -> bool {
            for (int r = 0, h = img.getHeight(); r < h; ++r)
                if (img.getPixelAt(c, r).getAlpha() > 0)
                    return false;
            return true;
        };
    const auto row_transparent =
        [&](int r) -> bool {
            for (int c = 0, w = img.getWidth(); c < w; ++c)
                if (img.getPixelAt(c, r).getAlpha() > 0)
                    return false;
            return true;
        };

    while (bounds.getWidth() > 0 && col_transparent(bounds.getRight()))
        bounds.removeFromRight(1);
    while (bounds.getHeight() > 0 && row_transparent(bounds.getBottom()))
        bounds.removeFromBottom(1);

    int nleft = 0;
    int ntop = 0;
    while (nleft < bounds.getWidth() && col_transparent(nleft))
        ++nleft;
    while (ntop < bounds.getHeight() && col_transparent(ntop))
        ++ntop;

    bounds.removeFromLeft(nleft);
    bounds.removeFromTop(ntop);
    return bounds;
}

}  // namespace Image_Utils
