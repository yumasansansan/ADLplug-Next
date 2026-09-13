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
#include <vector>

struct Emulator_Defaults;
const Emulator_Defaults &get_emulator_defaults();

struct Emulator_Defaults {
    // Names by emulator number, empty for the emulators this build lacks.
    StringArray choices;
    unsigned default_index = 0;

    bool is_built(unsigned index) const noexcept
    {
        return index < static_cast<unsigned>(choices.size()) && choices[static_cast<int>(index)].isNotEmpty();
    }
};

// The emulator to run for a number from a saved state or from the parameter.
// A number this build lacks -- a core left out by a build option, say -- is
// replaced by the default emulator for the same chip.
unsigned available_emulator(unsigned index);

// Icons for the emulator choices, by index. Only the editor shows them, and it
// holds them through a SharedResourcePointer, so they go away with the last
// editor. Keep them out of static storage: on Windows an image is a Direct2D
// resource, and releasing one while the module is being unloaded deadlocks.
struct Emulator_Icons {
    Emulator_Icons();
    std::vector<Image> images;
};

struct Chip_Settings {
    unsigned emulator = 0;
    unsigned chip_count = 2;
    unsigned fourop_count = 0;

    bool operator==(const Chip_Settings &) const = default;

    PropertySet to_properties() const;
    static Chip_Settings from_properties(const PropertySet &set);
};
