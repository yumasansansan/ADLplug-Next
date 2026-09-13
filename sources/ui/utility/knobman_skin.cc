//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

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
