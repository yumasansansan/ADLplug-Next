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
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

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

// The text of a field, which need not be UTF-8: a bank file names its banks and
// its instruments in bytes of its own, and a field keeps what it is given. The
// rule is the one JUCE reads text of an unknown encoding by -- UTF-8 when the
// bytes are UTF-8, Windows-1252 when they are not -- which is text that can be
// shown, and that the state of a project can keep and bring back.
//
// The rule is written out here rather than taken from String::createStringFromData,
// which reads the bytes of a file: it drops a byte order mark at the front, and
// reads the bytes after one as UTF-16 when the mark says so. A field is no file.
// Nothing put those bytes at the front to say an encoding, so a mark there is a
// character of the name like any other, and a bank file is free to hold one. To
// read it away would lose what the file holds, and would leave a field whose text
// is not the text of the field it was written to: the name would lose its first
// character every time a project was saved and read again. String::fromUTF8 does
// the reading once the bytes are known to be UTF-8, which is why the check comes
// first: it asserts what the check has just established, and every call here
// carries the length, so nothing reads past the field.
inline String name_from_field(std::span<const char> field)
{
    const std::string_view text = name_view(field);
    if (text.empty())
        return {};
    const auto bytes = static_cast<int>(text.size());
    if (CharPointer_UTF8::isValidString(text.data(), bytes))
        return String::fromUTF8(text.data(), bytes);

    // Windows-1252, character by character, as JUCE reads that code page. The
    // last character of the buffer is the zero that ends it, which is how a
    // CharPointer knows where the text stops.
    std::vector<juce_wchar> characters(text.size() + 1, 0);
    for (std::size_t i = 0; i < text.size(); ++i)
        characters[i] = CharacterFunctions::getUnicodeCharFromWindows1252Codepage(
            static_cast<std::uint8_t>(text[i]));
    return String(CharPointer_UTF32(characters.data()));
}

// Stores a name in a field, leaving out the characters which do not fit whole.
inline void copy_name_to_field(std::span<char> field, const String &name) noexcept
{
    const char *utf8 = name.toRawUTF8();
    const std::size_t length = utf8_fitting_length(utf8, field.size());
    std::fill(std::copy_n(utf8, length, field.begin()), field.end(), '\0');
}
