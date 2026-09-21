//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// SPDX-FileCopyrightText: 2018 Jean Pierre Cimalando
// SPDX-License-Identifier: BSL-1.0
//
// This file comes from ADLplug. The notice at the top is ADLplug's; the LICENSE
// it names was ADLplug's copy of the Boost Software License, now
// LICENSES/BSL-1.0.txt. The SPDX lines name its copyright holder and license in
// the machine-readable form of the REUSE specification.

#pragma once
#include "ui/components/waves.h"

class OPL3_Waves : public Waves
{
public:
    [[nodiscard]] unsigned wave_count() const override { return 8; }
    [[nodiscard]] double compute_wave(unsigned wave, double phase) const override;
};
