//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// SPDX-FileCopyrightText: 2018-2019 Jean Pierre Cimalando
// SPDX-License-Identifier: BSL-1.0
//
// This file comes from ADLplug. The notice at the top is ADLplug's; the LICENSE
// it names was ADLplug's copy of the Boost Software License, now
// LICENSES/BSL-1.0.txt. The SPDX lines name its copyright holder and license in
// the machine-readable form of the REUSE specification.

#pragma once
#include <math.h>

struct Vu_Monitor
{
    double p_ = 0;
    double mem_ = 0;
    void release(double t); // t = fs * release time
    double process(double x);
};

inline void Vu_Monitor::release(double t)
{
    p_ = exp(-1.0 / t);
}

inline double Vu_Monitor::process(double x)
{
    const double ax = fabs(x);
    const double p = p_;
    const double y = (ax > mem_) ? ax : p * mem_ + (1.0 - p) * ax;
    mem_ = y;
    return y;
}
