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
