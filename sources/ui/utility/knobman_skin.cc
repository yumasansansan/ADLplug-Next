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
    if (new_w <= 0 || new_h <= 0)
        return skin;

    // **Every frame in one image, drawn through one Graphics.** Rescaled one by
    // one, each frame was an image and a Graphics of its own, and on Windows an
    // image is a Direct2D bitmap and each of those makes a Direct2D device
    // context: some three per frame, 64 frames a skin, and the two small skins
    // were 0.6 s of the 0.65 s the editor took to open. Drawn into the cells of
    // one image, each frame is scaled as Image::rescaled scales it -- the same
    // drawImageTransformed, moved by whole pixels -- and the frames are the
    // cells clipped out of it, as the frames of a loaded skin are clipped out of
    // the strip they came from. The cells make a square rather than a column, to
    // keep the coordinates small, and a gap of two pixels keeps a frame's
    // resampling off its neighbours. Now 0.01 to 0.02 s a skin.
    //
    // **The same pixels, except with Direct2D.** Its high-quality scaling
    // depends on where in the target a bitmap is drawn, so on Windows the small
    // frames are not rescaled()'s to the pixel: measured, 2.8% of the knob's
    // pixels and 3.8% of the slider's differ, most of them by one or two levels,
    // at the anti-aliased edges of the shapes. The software renderer draws them
    // to the pixel alike (tests/unit/utility_tests.cc).
    const int count = static_cast<int>(frames.size());
    const int columns = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count))));
    const int rows = (count + columns - 1) / columns;
    constexpr int gap = 2;
    const int cell_w = new_w + gap;
    const int cell_h = new_h + gap;
    const auto cell = [&](int i) {
        return Rectangle<int>{(i % columns) * cell_w, (i / columns) * cell_h, new_w, new_h};
    };

    const Image &first = frames[0];
    const Image atlas(first.getPixelData()->createType()->create(first.getFormat(), columns * cell_w,
                                                                 rows * cell_h, true));
    {
        Graphics g(atlas);
        g.setImageResamplingQuality(Graphics::highResamplingQuality);
        for (int i = 0; i < count; ++i) {
            const Image &frame = frames[static_cast<std::size_t>(i)];
            const Rectangle<int> to = cell(i);
            const auto scale = AffineTransform::scale(
                static_cast<float>(new_w) / static_cast<float>(frame.getWidth()),
                static_cast<float>(new_h) / static_cast<float>(frame.getHeight()));
            g.drawImageTransformed(
                frame, scale.translated(static_cast<float>(to.getX()), static_cast<float>(to.getY())),
                false);
        }
    }

    skin->frames.reserve(frames.size());
    for (int i = 0; i < count; ++i)
        skin->frames.push_back(atlas.getClippedImage(cell(i)));
    return skin;
}
