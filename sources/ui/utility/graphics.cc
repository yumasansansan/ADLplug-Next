//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

#include "ui/utility/graphics.h"

namespace Graphics_Utils {

void draw_placeholder(Graphics &g, Rectangle<int> bounds)
{
    const Graphics::ScopedSaveState saved_state(g);
    g.setColour(Colour::fromRGB(0xff, 0x00, 0x00));
    g.drawRect(bounds);
    const Rectangle<float> area = bounds.toFloat();
    g.drawLine(Line<float>(area.getTopLeft(), area.getBottomRight()));
    g.drawLine(Line<float>(area.getTopRight(), area.getBottomLeft()));
}

}  // namespace Graphics_Utils
