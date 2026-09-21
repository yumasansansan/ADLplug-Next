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

#include "pak.h"
#include "JuceHeader.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

namespace {

constexpr char pak_magic[4] {'P', 'A', 'K', '2'};

std::uint32_t read_big_endian_u32(const std::uint8_t *p) noexcept
{
    return static_cast<std::uint32_t>(p[0]) << 24 | static_cast<std::uint32_t>(p[1]) << 16 |
           static_cast<std::uint32_t>(p[2]) << 8 | static_cast<std::uint32_t>(p[3]);
}

}  // namespace

bool Pak_File_Reader::init_with_data(const std::uint8_t *data, std::size_t size)
{
    data_ = data;
    size_ = size;
    return read_dictionary();
}

const std::string &Pak_File_Reader::name(std::size_t nth) const
{
    return entries_.at(nth).name;
}

std::vector<std::uint8_t> Pak_File_Reader::extract(std::size_t nth) const
{
    const Entry &entry = entries_.at(nth);
    return read_content(entry.offset, entry.size);
}

std::string Pak_File_Reader::info(std::size_t nth) const
{
    const Entry &entry = entries_.at(nth);
    const std::vector<std::uint8_t> text = read_content(entry.info_offset, entry.info_size);
    return {text.begin(), text.end()};
}

std::optional<std::size_t> Pak_File_Reader::find(std::string_view name) const
{
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].name == name)
            return i;
    }
    return std::nullopt;
}

// The largest an entry of a pack may say it is. A pack of this project holds bank
// files, and the formats bound one: a WOPL that names every bank and every
// instrument it can is some two megabytes. This is eight times that, and it is
// what keeps a pack from being taken at its word when it says two gigabytes. The
// piecewise reading below answers a pack that says more than the stream holds; it
// does not answer a stream that really can give it, since a few bytes of zlib
// inflate to as much as anyone likes, and the fuzzing of the readers found
// exactly that (fuzz/regressions holds the input).
constexpr std::uint32_t pak_content_size_max = 16u * 1024u * 1024u;

std::vector<std::uint8_t> Pak_File_Reader::read_content(std::uint32_t offset, std::uint32_t size) const
{
    static_assert(pak_content_size_max <= static_cast<std::uint32_t>(std::numeric_limits<int>::max()),
                  "the pieces are read in ints, as a JUCE stream reads them");
    if (size == 0 || size > pak_content_size_max)
        return {};

    MemoryInputStream mem_stream(data_ + content_offset_, size_ - content_offset_, false);
    GZIPDecompressorInputStream zlib_stream(&mem_stream, false, GZIPDecompressorInputStream::zlibFormat);

    if (!zlib_stream.setPosition(offset))
        return {};

    // The size is what the pack says, and a pack that says a gigabyte would have
    // a gigabyte taken for it before a byte had been read. The content is read in
    // pieces, into itself, and grows with what the stream really gives, so that a
    // size larger than the stream holds costs no more than what is there. The
    // room doubles rather than following the pieces, so that a bank of any size
    // is copied as few times as a vector ever copies it. A stream that gives less
    // than the pack said is no answer, as it was before.
    constexpr std::size_t piece_size = std::size_t{64} * 1024;
    std::vector<std::uint8_t> content;
    while (content.size() < size) {
        const std::size_t have = content.size();
        const auto want = static_cast<int>(std::min<std::size_t>(piece_size, size - have));
        content.reserve(std::max(have + static_cast<std::size_t>(want), 2 * content.capacity()));
        content.resize(have + static_cast<std::size_t>(want));
        if (zlib_stream.read(content.data() + have, want) != want)
            return {};
    }

    return content;
}

bool Pak_File_Reader::read_dictionary()
{
    entries_.clear();
    entries_.reserve(256);
    content_offset_ = 0;

    // A caller with nothing to give has nothing at all: no bytes, and no address
    // either. Neither is a pack, and memcmp() may not be given a null pointer
    // even for none of them.
    if (data_ == nullptr || size_ < sizeof pak_magic ||
        std::memcmp(data_, pak_magic, sizeof pak_magic) != 0)
        return false;

    const std::uint8_t *ptr = data_ + sizeof pak_magic;
    std::size_t left = size_ - sizeof pak_magic;
    const auto take_u32 = [&ptr, &left](std::uint32_t &value) {
        if (left < 4)
            return false;
        value = read_big_endian_u32(ptr);
        ptr += 4;
        left -= 4;
        return true;
    };

    for (;;) {
        Entry ent;

        if (!take_u32(ent.size))
            return false;
        if (ent.size == 0)
            break;
        if (!take_u32(ent.offset) || !take_u32(ent.info_size) || !take_u32(ent.info_offset))
            return false;

        const auto *name_end = static_cast<const std::uint8_t *>(std::memchr(ptr, 0, left));
        if (name_end == nullptr)
            return false;
        const auto name_length = static_cast<std::size_t>(name_end - ptr);
        ent.name.assign(ptr, name_end);
        ptr = name_end + 1;
        left -= name_length + 1;

        entries_.push_back(std::move(ent));
    }

    content_offset_ = static_cast<std::size_t>(ptr - data_);
    return true;
}
