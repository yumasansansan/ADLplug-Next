//     Part of ADLplug, distributed under the GNU GPL v3 or later.
//               (See accompanying file LICENSE.)

#include "test.h"
#include "utility/name_field.h"
#include "JuceHeader.h"
#include <cstddef>
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
