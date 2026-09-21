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
#include <numbers>

struct Dc_Filter {
    void cutoff(double f);
    double process(double in);
    double b0_ = 0;
    double p_ = 0;
    double last_in_ = 0;
    double last_out_ = 0;
};

inline void Dc_Filter::cutoff(double f)
{
    const double wn = std::numbers::pi_v<double> * f;
    const double b0 = b0_ = 1.0 / (1.0 + wn);
    p_ = (1.0 - wn) * b0;
}

inline double Dc_Filter::process(double in)
{
    in *= b0_;
    const double out = (in - last_in_) + p_ * last_out_;
    last_in_ = in;
    last_out_ = out;
    return out;
}
