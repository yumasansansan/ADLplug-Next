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

namespace Image_Utils {

Rectangle<int> get_image_solid_area(const Image &img);

// A small label to stand in for an icon that does not exist, such as the logo
// of an emulator core that has none.
Image make_text_icon(const String &text);

}  // namespace Image_Utils
