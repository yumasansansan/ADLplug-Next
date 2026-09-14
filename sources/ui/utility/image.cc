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

Image make_text_icon(const String &text)
{
    // Twice the height the editor shows the emulator icons at, so the label
    // stays sharp on high-density displays; opaque, so it reads on any
    // background.
    constexpr int height = 40;
    constexpr float padding = 10.0f;
    constexpr float corner = 6.0f;
    const Font font(FontOptions(26.0f, Font::bold));
    const int width = static_cast<int>(std::ceil(GlyphArrangement::getStringWidth(font, text) + 2.0f * padding));

    Image image(Image::ARGB, std::max(width, height), height, true);
    {
        Graphics g(image);
        const Rectangle<float> frame = image.getBounds().toFloat().reduced(1.0f);
        g.setColour(Colour(0xff4f5d64));
        g.fillRoundedRectangle(frame, corner);
        g.setColour(Colour(0xffa3b3ba));
        g.drawRoundedRectangle(frame, corner, 2.0f);
        g.setColour(Colours::white);
        g.setFont(font);
        g.drawText(text, image.getBounds(), Justification::centred, false);
    }
    return image;
}

Rectangle<int> get_image_solid_area(const Image &img)
{
    Rectangle<int> bounds = img.getBounds();

    auto col_transparent =
        [&](int c) -> bool {
            for (int r = 0, h = img.getHeight(); r < h; ++r)
                if (img.getPixelAt(c, r).getAlpha() > 0)
                    return false;
            return true;
        };
    auto row_transparent =
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
