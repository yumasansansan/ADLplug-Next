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

#include "ui/components/indicator_NxM.h"

Indicator_NxM::Indicator_NxM(unsigned rows, unsigned cols)
    : rows_(rows), columns_(cols), bits_(std::size_t{rows} * cols)
{
}

bool Indicator_NxM::value(unsigned row, unsigned column) const
{
    const std::size_t index = index_from(row, column);
    return index < bits_.size() && bits_[index];
}

void Indicator_NxM::set_value(unsigned row, unsigned column, bool value)
{
    const std::size_t index = index_from(row, column);
    if (index >= bits_.size())
        return;
    if (bits_[index] != value) {
        bits_[index] = value;
        repaint();
    }
}

void Indicator_NxM::paint(Graphics &g)
{
    const unsigned rows = rows_;
    const unsigned columns = columns_;
    if (rows == 0 || columns == 0)
        return;

    const LookAndFeel &lnf = getLookAndFeel();
    const Colour colour_on = Colour::fromRGBA(0xdf, 0xf0, 0xff, 0xff);
    const Colour colour_off = lnf.findColour(Label::backgroundColourId);
    const Colour colour_outline = Colour::fromRGBA(0x8e, 0x98, 0x9b, 0xff);

    const Rectangle<double> bounds = getLocalBounds().toDouble();
    const double w1 = bounds.getWidth() / columns;
    const double h1 = bounds.getHeight() / rows;
    for (unsigned r = 0; r < rows; ++r) {
        for (unsigned c = 0; c < columns; ++c) {
            const Rectangle<float> rect = Rectangle<double>(
                bounds.getX() + c * w1, bounds.getY() + r * h1, w1, h1).reduced(1.0).toFloat();
            g.setColour(value(r, c) ? colour_on : colour_off);
            g.fillRect(rect);
            g.setColour(colour_outline);
            g.drawRect(rect);
        }
    }
}

std::size_t Indicator_NxM::index_from(unsigned row, unsigned column) const
{
    if (row >= rows_ || column >= columns_)
        return bits_.size();
    return std::size_t{rows_} * column + row;
}
