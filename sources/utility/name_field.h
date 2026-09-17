// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

#pragma once
#include "JuceHeader.h"
#include <algorithm>
#include <cstddef>
#include <span>
#include <string_view>

// The names of banks and instruments are fixed-size fields of UTF-8 text,
// zero-filled, and terminated only when shorter than the field.

// The length of the text in the first `max` bytes of a UTF-8 string, less a
// character which the limit would cut. Reads no further than `max` bytes, and
// no further than the terminator of a shorter string: nothing is computed past
// that, not even a pointer, as text + max would be.
inline std::size_t utf8_fitting_length(const char *text, std::size_t max) noexcept
{
    std::size_t length = 0;
    while (length < max && text[length] != '\0')
        ++length;
    if (length < max)
        return length;

    // Go back to the first byte of the last character, and see if it is whole.
    std::size_t start = length;
    while (start > 0 && length - start < 3 && (static_cast<unsigned char>(text[start - 1]) & 0xc0) == 0x80)
        --start;
    if (start == 0)
        return length;
    const auto lead = static_cast<unsigned char>(text[start - 1]);
    const std::size_t size = (lead >= 0xf0) ? 4 : (lead >= 0xe0) ? 3 : (lead >= 0xc0) ? 2 : 1;
    return (start - 1 + size > length) ? start - 1 : length;
}

// The text of a field.
inline std::string_view name_view(std::span<const char> field) noexcept
{
    const auto end = std::find(field.begin(), field.end(), '\0');
    return {field.data(), static_cast<std::size_t>(end - field.begin())};
}

inline String name_from_field(std::span<const char> field)
{
    const std::string_view text = name_view(field);
    return String::fromUTF8(text.data(), static_cast<int>(text.size()));
}

// Stores a name in a field, leaving out the characters which do not fit whole.
inline void copy_name_to_field(std::span<char> field, const String &name) noexcept
{
    const char *utf8 = name.toRawUTF8();
    const std::size_t length = utf8_fitting_length(utf8, field.size());
    std::fill(std::copy_n(utf8, length, field.begin()), field.end(), '\0');
}
