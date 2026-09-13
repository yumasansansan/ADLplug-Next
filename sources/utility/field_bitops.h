//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

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
