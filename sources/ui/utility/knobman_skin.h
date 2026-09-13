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
#include <cstddef>
#include <vector>

class Km_Skin;
using Km_Skin_Ptr = ReferenceCountedObjectPtr<Km_Skin>;

enum Km_Style
{
    Km_Rotary,
    Km_LinearHorizontal,
};

class Km_Skin : public ReferenceCountedObject
{
public:
    Km_Style style = Km_Rotary;
    std::vector<Image> frames;
    explicit operator bool() const { return !frames.empty(); }

    // Cuts a vertical strip into frame_count frames, then crops the border
    // which is transparent in all of them.
    void load(const Image &img, int frame_count);
    void load_data(const void *data, std::size_t size, int frame_count);

    Km_Skin_Ptr scaled(double ratio) const;

private:
    JUCE_LEAK_DETECTOR(Km_Skin)
};
