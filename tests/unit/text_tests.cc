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
#include <span>
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

    // Text whose end is its own, rather than a terminator's: what is past it is
    // no part of it, however much room the limit leaves.
    CHECK(utf8_fitting_length(std::string_view(full, 3), 32) == 3);
    CHECK(utf8_fitting_length(std::string_view(full, 3), 2) == 2);
    CHECK(utf8_fitting_length(std::string_view(three), 3) == 1);
    CHECK(utf8_fitting_length(std::string_view(three), 4) == 4);
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

ADLPLUG_TEST(name_field_keeps_a_mark_as_a_character)
{
    // A field is no file. Nothing put its bytes there to say an encoding, so the
    // three bytes of a byte order mark at the front of one are a character of the
    // name like any other, which a bank file is free to hold. A field of two of
    // them is what found this, in the long fuzzing of the readers of small
    // things: the first reading gave one mark back, and the reading after that
    // gave none, so the name a project was saved with was not the name it came
    // back with.
    const char mark[3] = {'\xef', '\xbb', '\xbf'};
    CHECK(name_from_field(mark) == String::fromUTF8("\xef\xbb\xbf"));

    const char field[6] = {'\xef', '\xbb', '\xbf', '\xef', '\xbb', '\xbf'};
    char stored[6] {};
    copy_name_to_field(stored, name_from_field(field));
    CHECK(name_view(stored) == std::string_view("\xef\xbb\xbf\xef\xbb\xbf"));
    char again[6] {};
    copy_name_to_field(again, name_from_field(stored));
    CHECK(std::ranges::equal(stored, again));

    // Nor is a field ever UTF-16, whatever it begins with: the two bytes that
    // would say so are two characters, as every other byte of a field that is
    // not UTF-8 is one.
    const char wide[4] = {'\xff', '\xfe', 'A', '\0'};
    CHECK(name_from_field(wide) == String::fromUTF8("\xc3\xbf\xc3\xbe" "A"));
}

ADLPLUG_TEST(name_field_from_bytes)
{
    // The audio thread puts a name in a field without a String in between, and
    // the rule is the one text is read by: UTF-8 as it stands, anything else as
    // Windows-1252, a character to a byte. Both ways of filling a field agree.
    const char utf8[] = "ab\xc3\xa9";
    char from_bytes[8] {};
    char from_text[8] {};
    copy_name_bytes_to_field(from_bytes, std::span(utf8, 4));
    copy_name_to_field(from_text, name_from_field(utf8));
    CHECK(std::ranges::equal(from_bytes, from_text));

    const char other[] = "ab\xe9";  // the same letter, in Windows-1252
    char other_bytes[8] {};
    char other_text[8] {};
    copy_name_bytes_to_field(other_bytes, std::span(other, 3));
    copy_name_to_field(other_text, name_from_field(other));
    CHECK(std::ranges::equal(other_bytes, other_text));

    // What is past a name is not the name. A name of three bytes, in a buffer
    // that goes on without a terminator, fills a larger field with those three
    // and with nothing that follows them.
    const char buffer[8] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
    char field[8] {};
    copy_name_bytes_to_field(field, std::span(buffer, 3));
    CHECK(name_view(field) == "abc");

    // A character the field would cut is left out here as well.
    const char wide[] = "ab\xe3\x81\x82";
    char small[4] {};
    copy_name_bytes_to_field(small, std::span(wide, 5));
    CHECK(name_view(small) == "ab");
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
