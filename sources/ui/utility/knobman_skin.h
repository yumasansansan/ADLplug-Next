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

    [[nodiscard]] Km_Skin_Ptr scaled(double ratio) const;

private:
    JUCE_LEAK_DETECTOR(Km_Skin)
};
