// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// The small readers: the pack of banks the plugin carries, the fields a name
// sits in, and the configuration file. None of them plays a note, and all three
// read bytes the plugin did not write -- a pack out of a binary that may have
// been damaged, names out of bank files that hold whatever bytes they hold, and
// a configuration file that anyone may edit.
//
// A record is one byte, and the bits at its top say what it is:
//
//   00xxxxxx  a pack: the two bytes that follow say how many bytes of it there
//             are, and those bytes go to Pak_File_Reader. Every bank it finds is
//             then read -- its name, its file, its text -- and looked up again by
//             name. With the lowest bit of x, the reader is given a null pointer
//             for that length instead of the bytes, which is what a caller with
//             nothing to give has; with the second bit, the rest of the input is
//             the pack and no length is read, which is how a whole pack of the
//             plugin's own is a seed of one byte and the pack
//   01xxxxxx  a field of 1 + x bytes, filled with the bytes that follow: what
//             comes out of it goes back in, and the field that makes must not
//             change when the same is done again
//   10xxxxxx  a name of x bytes, the bytes that follow, stored in a field of as
//             many bytes as the byte after them says -- which may be none, since
//             a field of no bytes is a field a caller may have: what the field
//             keeps must be text that fits whole, and reading it back and storing
//             it again must leave the field as it was
//   11xxxxxx  a configuration file: the two bytes that follow say how many bytes
//             of it there are, and those bytes are loaded as one. The values the
//             plugin asks of it are read, it is saved and loaded again, and the
//             same values must come back. With the lowest bit of x the loader is
//             given a null pointer for that length instead of the bytes, and with
//             the second the rest of the input is the file
//
// The configuration goes through SimpleIni the way Configuration::load_ini does
// -- SetUnicode, then LoadData over the bytes of the file -- rather than through
// Configuration itself, which includes the key map, which includes the keyboard
// of the editor, which wants the interface modules of JUCE that a fuzz target
// does without (fuzz/CMakeLists.txt says more). The bytes are what comes from
// outside, and this is what reads them. Nothing here touches the file of the
// person running it: Configuration::load_default and save_default do, and this
// target calls neither.

#include "fuzz.h"
#include "utility/pak.h"
#include "utility/name_field.h"
#include "JuceHeader.h"
#include <SimpleIni.h>
#include <algorithm>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace {

// How many bytes of the input one record may give a reader. Two bytes say the
// length, so an input may hand over as much as it holds.
constexpr std::size_t length_max = 0xffff;

class Input {
public:
    Input(const std::uint8_t *data, std::size_t size) noexcept
        : data_(data), size_(size) {}

    [[nodiscard]] bool done() const noexcept
        { return at_ >= size_; }

    std::uint8_t byte() noexcept
        { return (at_ < size_) ? data_[at_++] : 0; }

    // The next `count` bytes, whatever they are, with zeros past the end of the
    // input as everywhere else.
    std::vector<std::uint8_t> bytes(std::size_t count)
    {
        std::vector<std::uint8_t> out;
        out.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
            out.push_back(byte());
        return out;
    }

    // How many bytes are left, for a record that takes all of them.
    [[nodiscard]] std::size_t left() const noexcept
        { return (at_ < size_) ? size_ - at_ : 0; }

    // How long the bytes of a reader's input are: the rest of the input, or the
    // two bytes that say so. The two are read one after the other rather than in
    // one expression, where nothing would say which of them comes first.
    std::size_t length(bool the_rest) noexcept
    {
        if (the_rest)
            return left();
        const std::size_t high = byte();
        const std::size_t low = byte();
        return std::min(length_max, high << 8 | low);
    }

private:
    const std::uint8_t *data_;
    std::size_t size_;
    std::size_t at_ = 0;
};

// Every bank a pack says it holds, read the way the plugin reads them: the name,
// the file, and the text. A reader that says it read the pack has to answer for
// every entry it counted.
void read_the_pack(const std::uint8_t *data, std::size_t size)
{
    Pak_File_Reader pak;
    if (!pak.init_with_data(data, size))
        return;

    const std::size_t count = pak.entry_count();
    for (std::size_t i = 0; i < count; ++i) {
        const std::string &name = pak.name(i);
        // A name the reader gave back is one it finds again, and what it finds is
        // the first bank of that name.
        const std::optional<std::size_t> found = pak.find(name);
        FUZZ_CHECK(found.has_value());
        FUZZ_CHECK(*found <= i);
        FUZZ_CHECK(pak.name(*found) == name);

        // Reading a bank twice gives the same bytes: what comes out of a pack is
        // what the pack holds, and not what the read before it left behind.
        const std::vector<std::uint8_t> file = pak.extract(i);
        FUZZ_CHECK(pak.extract(i) == file);
        const std::string info = pak.info(i);
        FUZZ_CHECK(pak.info(i) == info);
    }
    // Reading the banks leaves the pack as it was.
    FUZZ_CHECK(pak.entry_count() == count);
}

// A field of bytes, read as a name and stored again. The first turn may change
// the bytes -- a field keeps whatever a bank file put in it, and the text of it
// is made the way JUCE reads bytes whose encoding is not known -- so what has to
// hold is that the second turn changes nothing.
void the_field_that_holds_a_name(std::vector<char> field)
{
    const String first = name_from_field(field);
    std::vector<char> stored = field;
    copy_name_to_field(stored, first);

    const String second = name_from_field(stored);
    std::vector<char> again = stored;
    copy_name_to_field(again, second);
    FUZZ_CHECK(again == stored);
    FUZZ_CHECK(name_from_field(again) == second);

    // A field holds text and then nothing: what is read from it stops at the
    // first zero, and every byte past the text is a zero.
    const std::string_view text = name_view(stored);
    FUZZ_CHECK(text.size() <= stored.size());
    FUZZ_CHECK(std::all_of(stored.begin() + static_cast<std::ptrdiff_t>(text.size()), stored.end(),
                           [](char c) { return c == '\0'; }));
}

// A name stored in a field of the size the input chose, whatever the bytes of
// the name are, and whether or not there is a field at all: a field of no bytes
// is a field, and the length of the text that fits in none of them is none.
void the_name_that_goes_into_a_field(const std::vector<std::uint8_t> &name_bytes, std::size_t field_size)
{
    const String name = String::createStringFromData(name_bytes.data(),
                                                     static_cast<int>(name_bytes.size()));
    FUZZ_CHECK(utf8_fitting_length(name.toRawUTF8(), 0) == 0);
    // The field starts as bytes that are not zeros, so that what was written and
    // what was cleared can be told apart.
    std::vector<char> field(field_size, '\1');
    copy_name_to_field(field, name);

    // What was stored is the first characters of the name that fit, and nothing
    // of what did not.
    const char *utf8 = name.toRawUTF8();
    const std::size_t fitting = utf8_fitting_length(utf8, field_size);
    FUZZ_CHECK(fitting <= field_size);
    FUZZ_CHECK(name_view(field).size() == fitting);
    FUZZ_CHECK(std::equal(field.begin(), field.begin() + static_cast<std::ptrdiff_t>(fitting), utf8));

    // And what is in the field now is a field that holds a name.
    the_field_that_holds_a_name(std::move(field));
}

// The keys the plugin asks the configuration for, and the version it looks at
// before it decides whether the file is one of its own. A value that is not
// there is the default, which is how the plugin tells the two apart: the pointer
// it gets back is the very one it offered.
constexpr const char *absent = "\x01 not in the file \x01";

String values_of(const CSimpleIniA &ini)
{
    String all;
    const auto ask = [&all, &ini](const char *section, const char *key) {
        const char *value = ini.GetValue(section, key, absent);
        all << section << '/' << key << '=' << (value ? value : "(none)") << '\n';
    };
    ask("", "configuration-version");
    ask("paths", "last-instrument-directory");
    ask("piano", "layout");
    ask("piano", "keymap:qwerty");
    ask("piano", "keymap:azerty");
    all << "version=" << String(ini.GetLongValue("", "configuration-version")) << '\n';
    return all;
}

void the_configuration_file(const char *data, std::size_t size)
{
    CSimpleIniA ini;
    ini.SetUnicode();
    if (ini.LoadData(data, size) != SI_OK) {
        // Nothing was loaded, so nothing is there to answer: the value of a key
        // is the pointer the caller offered.
        FUZZ_CHECK(ini.GetValue("piano", "layout", absent) == absent);
        return;
    }

    const String values = values_of(ini);

    // What the plugin writes back is what it read: a value that came out of a
    // file it was given has to survive being saved to one of its own, or the
    // configuration of whoever runs it would change by being opened.
    std::string saved;
    FUZZ_CHECK(ini.Save(saved, false) == SI_OK);

    CSimpleIniA again;
    again.SetUnicode();
    FUZZ_CHECK(again.LoadData(saved.data(), saved.size()) == SI_OK);
    FUZZ_CHECK(values_of(again) == values);
}

}  // namespace

int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    // An input that begins with the four bytes a pack begins with is a pack,
    // whole. The pack the build makes for the plugin is then a seed as it is --
    // no fuzzer would come upon the zlib stream inside it -- and what a fuzzer
    // makes of that seed stays a pack for as long as it keeps those four bytes.
    // Anything else is the records above, which can hold a pack as well.
    if (size >= 4 && std::memcmp(data, "PAK2", 4) == 0) {
        read_the_pack(data, size);
        return 0;
    }

    Input input(data, size);
    while (!input.done()) {
        const std::uint8_t record = input.byte();
        const unsigned value = record & 0x3fu;

        switch (record >> 6) {
        case 0: {
            const std::vector<std::uint8_t> pack = input.bytes(input.length((value & 2u) != 0));
            read_the_pack(((value & 1u) == 0) ? pack.data() : nullptr, pack.size());
            break;
        }
        case 1: {
            const std::vector<std::uint8_t> field = input.bytes(1u + value);
            the_field_that_holds_a_name(std::vector<char>(field.begin(), field.end()));
            break;
        }
        case 2: {
            const std::vector<std::uint8_t> name = input.bytes(value);
            the_name_that_goes_into_a_field(name, input.byte() & 0x3fu);
            break;
        }
        default: {
            const std::vector<std::uint8_t> file = input.bytes(input.length((value & 2u) != 0));
            // The configuration is text to its reader and bytes here: that the
            // bytes need not be text is the point of giving them to it.
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            the_configuration_file(((value & 1u) == 0) ? reinterpret_cast<const char *>(file.data()) : nullptr,
                                   file.size());
            break;
        }
        }
    }
    return 0;
}
