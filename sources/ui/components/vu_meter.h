//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// SPDX-FileCopyrightText: 2018, 2021 Jean Pierre Cimalando
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

class Vu_Meter : public Component
{
public:
    Vu_Meter();
    explicit Vu_Meter(const String &name);

    [[nodiscard]] double value() const
        { return value_; }
    void set_value(double value);

    [[nodiscard]] bool logarithmic() const
        { return logarithmic_; }
    void set_logarithmic(bool logarithmic);

    void set_hue(double start, double range);
    void set_num_stops(unsigned num_stops);

protected:
    void paint(Graphics &g) override;

private:
    void update_gradient();


    double value_ = 1.0;
    bool logarithmic_ = false;
    double hue_start_ = 0.0;
    double hue_range_ = 0.0;
    unsigned num_stops_ = 10;
    ColourGradient gradient_;
};
