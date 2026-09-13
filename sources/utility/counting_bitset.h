//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

#pragma once
#include <bitset>
#include <cstddef>
#include <memory>
#include <string>

// A std::bitset that keeps its population count, so count() is constant time.
template <std::size_t N>
class counting_bitset {
public:
    bool operator==(const counting_bitset &) const = default;

    bool test(std::size_t pos) const
        { return bits_.test(pos); }

    bool all() const noexcept
        { return count_ == N; }
    bool any() const noexcept
        { return count_ > 0; }
    bool none() const noexcept
        { return count_ == 0; }

    std::size_t count() const noexcept
        { return count_; }

    counting_bitset &set() noexcept
    {
        count_ = N;
        bits_.set();
        return *this;
    }

    counting_bitset &set(std::size_t pos, bool value = true)
    {
        if (bits_.test(pos) != value) {
            if (value)
                ++count_;
            else
                --count_;
            bits_.set(pos, value);
        }
        return *this;
    }

    counting_bitset &reset() noexcept
    {
        count_ = 0;
        bits_.reset();
        return *this;
    }

    counting_bitset &reset(std::size_t pos)
        { return set(pos, false); }

    counting_bitset &flip() noexcept
    {
        count_ = N - count_;
        bits_.flip();
        return *this;
    }

    counting_bitset &flip(std::size_t pos)
        { return set(pos, !bits_.test(pos)); }

    template <class CharT = char, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT>>
    std::basic_string<CharT, Traits, Allocator> to_string(CharT zero = CharT('0'), CharT one = CharT('1')) const
        { return bits_.template to_string<CharT, Traits, Allocator>(zero, one); }

private:
    std::size_t count_ = 0;
    std::bitset<N> bits_;
};
