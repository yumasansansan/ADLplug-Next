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

// Reads and writes bit fields inside the register bytes of an instrument.
// `shift` and `size` locate the field; the *_inverted variants store the
// field as (max - value), which is how the chips encode levels and rates.
namespace Field_Bitops {

template <unsigned shift, unsigned size, class T_result, class T_field>
constexpr T_result get(T_field x) noexcept
{
    const unsigned mask = (1u << size) - 1;
    return static_cast<T_result>((static_cast<unsigned>(x) >> shift) & mask);
}

template <unsigned shift, unsigned size, class T_value, class T_field>
constexpr void set(T_field &x, T_value v) noexcept
{
    const unsigned mask = (1u << size) - 1;
    x = static_cast<T_field>((static_cast<unsigned>(x) & ~(mask << shift)) |
                             ((static_cast<unsigned>(v) & mask) << shift));
}

template <unsigned shift, unsigned size, class T_result, class T_field>
constexpr T_result get_inverted(T_field x) noexcept
{
    const unsigned max = (1u << size) - 1;
    return static_cast<T_result>(max - get<shift, size, unsigned>(x));
}

template <unsigned shift, unsigned size, class T_value, class T_field>
constexpr void set_inverted(T_field &x, T_value v) noexcept
{
    const unsigned max = (1u << size) - 1;
    set<shift, size>(x, max - static_cast<unsigned>(v));
}

}  // namespace Field_Bitops
