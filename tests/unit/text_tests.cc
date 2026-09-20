// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

#include "test.h"
#include "utility/name_field.h"
#include "JuceHeader.h"
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <string_view>

ADLPLUG_TEST(utf8_fitting_length)
{
    // Shorter than the limit: the text up to its terminator.
    CHECK(utf8_fitting_length("abc", 32) == 3);
    // As long as the limit, with no terminator in it.
    const char full[4] = {'a', 'b', 'c', 'd'};
    CHECK(utf8_fitting_length(full, 4) == 4);

    // A character the limit would cut is left out: two bytes ("abé")...
    const char two[] = "ab\xc3\xa9";
    CHECK(utf8_fitting_length(two, 3) == 2);
    CHECK(utf8_fitting_length(two, 4) == 4);
    // ...three ("aあ")...
    const char three[] = "a\xe3\x81\x82";
    CHECK(utf8_fitting_length(three, 2) == 1);
    CHECK(utf8_fitting_length(three, 3) == 1);
    CHECK(utf8_fitting_length(three, 4) == 4);
    // ...and four (a musical keyboard, U+1F3B9).
    const char four[] = "\xf0\x9f\x8e\xb9";
    for (std::size_t limit = 1; limit < 4; ++limit)
        CHECK(utf8_fitting_length(four, limit) == 0);
    CHECK(utf8_fitting_length(four, 4) == 4);

    // Stray continuation bytes are not a character to keep whole.
    const char stray[4] = {'\x80', '\x80', '\x80', '\x80'};
    CHECK(utf8_fitting_length(stray, 4) == 4);
}

ADLPLUG_TEST(name_field)
{
    // The field keeps the characters that fit whole, and zeros after them.
    char field[8];
    copy_name_to_field(field, String::fromUTF8("\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86"));
    CHECK(name_view(field) == std::string_view("\xe3\x81\x82\xe3\x81\x84"));
    CHECK(field[6] == '\0' && field[7] == '\0');
    CHECK(name_from_field(field) == String::fromUTF8("\xe3\x81\x82\xe3\x81\x84"));

    // A name as long as the field fills it, without a terminator.
    copy_name_to_field(field, "12345678");
    CHECK(name_view(field) == "12345678");

    copy_name_to_field(field, "");
    CHECK(name_view(field).empty());
}

ADLPLUG_TEST(name_field_of_other_bytes)
{
    // A bank file names its banks and instruments in bytes of its own, which
    // need not be UTF-8, and a field keeps them as they are. Turning such a
    // field into text gives text all the same -- the bytes read as
    // Windows-1252, the way JUCE reads text whose encoding it does not know --
    // so that it can be shown, and written into the state of a project and read
    // back.
    const char latin[8] = {'\xe9', 't', 'u', 'd', 'e', '\0', '\0', '\0'};
    CHECK(name_from_field(latin) == String::fromUTF8("\xc3\xa9tude"));

    // Bytes that are no text at all still come back as text, and as valid
    // UTF-8: a state that held them is read back the same.
    const char stray[4] = {'\x80', '\x81', '\x82', '\x83'};
    const String text = name_from_field(stray);
    CHECK(text.isNotEmpty());
    char again[16] {};
    copy_name_to_field(again, text);
    CHECK(name_from_field(again) == text);
}

ADLPLUG_TEST(name_field_holds_its_own_text)
{
    // The text of bytes that are not UTF-8 is longer than the bytes: every
    // byte of a field can become two or three. A field that keeps text put
    // there from its own text holds it whole, whatever the bytes were -- which
    // is what lets the state of a project keep a name and bring it back.
    for (unsigned first = 0; first < 256; ++first) {
        char bytes[8];
        for (unsigned i = 0; i < sizeof bytes; ++i)
            bytes[i] = static_cast<char>((first + i * 37u) & 0xffu);

        char field[8] {};
        copy_name_to_field(field, name_from_field(bytes));
        char again[8] {};
        copy_name_to_field(again, name_from_field(field));
        if (!std::ranges::equal(field, again)) {
            std::fprintf(stderr, "  a field of the bytes from %u does not keep its own text\n", first);
            CHECK(false);
            return;
        }
    }
    CHECK(true);
}
