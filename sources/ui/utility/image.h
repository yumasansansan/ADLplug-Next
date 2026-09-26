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

#pragma once
#include "JuceHeader.h"
#include <vector>

namespace Image_Utils {

// juce::, for a translation unit that has <windows.h>, which has a Rectangle.
juce::Rectangle<int> get_image_solid_area(const Image &img);

// A small label to stand in for an icon that does not exist, such as the logo
// of an emulator core that has none, in an image of the given type.
Image make_text_icon(const String &text, const ImageType &type = NativeImageType());

// The labels make_text_icon draws for all of `texts`, drawn into one image
// through one Graphics and handed out as parts of it: one image and one
// Graphics, where a label apiece was one of each (see Km_Skin::scaled).
std::vector<Image> make_text_icons(const StringArray &texts,
                                   const ImageType &type = NativeImageType());

}  // namespace Image_Utils
