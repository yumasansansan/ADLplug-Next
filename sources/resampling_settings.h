// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

#pragma once
#include "JuceHeader.h"
#include "resample.hpp"
#include <array>
#include <cstdint>
#include <string>

// Where the chip's samples become the host's, and how (sources/utility/chip_resampler.h).
//
// Every parameter of the filter is the user's to choose, as MediaPerch leaves
// them: the named settings are places to start from rather than the only choices,
// and no limit is set here that the design does not set itself. A design that
// cannot be built -- more coefficients than its own max_taps, a method refused at
// that length -- says so in words, and the plugin then leaves the conversion to
// the library, as it did before there was a choice.
//
// These are not parameters of the plugin. A parameter is what a host automates,
// and every change of these designs a filter again, which takes a noticeable part
// of a second and memory to hold it; they are part of the state instead, like the
// banks, and a change is applied by the worker rather than by the audio thread.
struct Resampling_Settings {
    // Here, through a filter designed for the pair of rates; or in the library,
    // by a straight line between one sample and the next, as it always did.
    bool own_filter = true;
    mp::resample::Design design;

    bool operator==(const Resampling_Settings &) const = default;

    [[nodiscard]] PropertySet to_properties() const;
    static Resampling_Settings from_properties(const PropertySet &set);
};

// What became of the settings when the player was made, for the editor to show:
// whether the plugin's own filter is running, what the design measured of itself,
// and what it said when it refused. Copied as its bytes through the queue to the
// editor, so the words are a field of fixed size.
struct Resampling_Status {
    bool active = false;
    unsigned chip_rate = 0;
    unsigned host_rate = 0;
    double stopband_db = 0.0;
    double passband_ripple_db = 0.0;
    // Per frame of the host's, per channel.
    double multiplies = 0.0;
    // What the filter holds, which is the memory it takes, eight bytes apiece.
    std::uint64_t coefficients = 0;
    char why[240] {};
};

// The named settings of the resampler, as it takes them by name
// (mp::resample::design_from_name), in the order they are offered. The test of
// the settings checks that each is still a name it takes.
inline constexpr std::array<const char *, 4> resampling_preset_names {"fast", "good", "best", "extreme"};

// The named setting that `design` is exactly, or an empty string when it is none
// of them -- which is what a design changed by hand is.
[[nodiscard]] std::string resampling_preset_of(const mp::resample::Design &design);

// The design a named setting stands for, from the defaults of every other field.
[[nodiscard]] bool resampling_preset(const std::string &name, mp::resample::Design &design);
