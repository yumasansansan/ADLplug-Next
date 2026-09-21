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
#include <vector>

struct Emulator_Defaults;
const Emulator_Defaults &get_emulator_defaults();

struct Emulator_Defaults {
    // Names by emulator number, empty for the emulators this build lacks.
    StringArray choices;
    unsigned default_index = 0;

    [[nodiscard]] bool is_built(unsigned index) const noexcept
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
    // How the library takes a channel of the chip for a new note: -1 is the
    // choice it makes for itself, out of the volume model and the kind of music,
    // and the modes above it are the ways of making that choice (ADLMIDI_ChanAlloc_*).
    int chan_alloc = -1;

    bool operator==(const Chip_Settings &) const = default;

    [[nodiscard]] PropertySet to_properties() const;
    static Chip_Settings from_properties(const PropertySet &set);
};
