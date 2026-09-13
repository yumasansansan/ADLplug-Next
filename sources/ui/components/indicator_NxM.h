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
