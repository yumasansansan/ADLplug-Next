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
#include "JuceHeader.h"
#include <cstddef>
#include <vector>

class Indicator_NxM : public Component
{
public:
    Indicator_NxM(unsigned rows, unsigned cols);

    bool value(unsigned row, unsigned column) const;
    void set_value(unsigned row, unsigned column, bool value);

    unsigned rows() const
        { return rows_; }
    unsigned columns() const
        { return columns_; }

protected:
    void paint(Graphics &g) override;

private:
    unsigned rows_ = 0;
    unsigned columns_ = 0;
    std::vector<bool> bits_;
    // Past the end of bits_ for a cell outside the grid.
    std::size_t index_from(unsigned row, unsigned column) const;
};
