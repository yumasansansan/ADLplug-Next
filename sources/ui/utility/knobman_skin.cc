//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// SPDX-FileCopyrightText: 2018-2019 Jean Pierre Cimalando
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

#include "ui/utility/knobman_skin.h"
#include "ui/utility/image.h"
#include <cmath>

void Km_Skin::load(const Image &img, int frame_count)
{
    frames.clear();
    if (frame_count <= 0 || !img.isValid())
        return;

    const int w = img.getWidth();
    const int hframe = img.getHeight() / frame_count;
    frames.reserve(static_cast<std::size_t>(frame_count));
    for (int i = 0; i < frame_count; ++i)
        frames.push_back(img.getClippedImage({0, i * hframe, w, hframe}));

    // crop transparent bounds
    Rectangle<int> opaque_bounds = Image_Utils::get_image_solid_area(frames[0]);
    for (const Image &frame : frames)
        opaque_bounds = opaque_bounds.getUnion(Image_Utils::get_image_solid_area(frame));
    for (Image &frame : frames)
        frame = frame.getClippedImage(opaque_bounds);
}

void Km_Skin::load_data(const void *data, std::size_t size, int frame_count)
{
    load(ImageFileFormat::loadFrom(data, size), frame_count);
}

Km_Skin_Ptr Km_Skin::scaled(double ratio) const
{
    Km_Skin_Ptr skin = new Km_Skin;
    skin->style = style;
    if (frames.empty())
        return skin;

    const int new_w = static_cast<int>(std::lround(frames[0].getWidth() * ratio));
    const int new_h = static_cast<int>(std::lround(frames[0].getHeight() * ratio));
    skin->frames.reserve(frames.size());
    for (const Image &frame : frames)
        skin->frames.push_back(frame.rescaled(new_w, new_h, Graphics::highResamplingQuality));

    return skin;
}
