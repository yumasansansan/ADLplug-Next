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
#include <cstring>
#include <limits>

namespace {

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

std::string Pak_File_Reader::extract(std::size_t nth) const
{
    const Entry &entry = entries_.at(nth);
    if (entry.size > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
        return {};

    MemoryInputStream mem_stream(data_ + content_offset_, size_ - content_offset_, false);
    GZIPDecompressorInputStream gz_stream(&mem_stream, false, GZIPDecompressorInputStream::gzipFormat);

    if (!gz_stream.setPosition(entry.offset))
        return {};

    std::string data(entry.size, '\0');
    const int size = static_cast<int>(entry.size);
    if (gz_stream.read(data.data(), size) != size)
        return {};

    return data;
}

bool Pak_File_Reader::read_dictionary()
{
    entries_.clear();
    entries_.reserve(256);
    content_offset_ = 0;

    const std::uint8_t *ptr = data_;
    std::size_t left = size_;
    for (;;) {
        Entry ent;

        if (left < 4)
            return false;
        ent.size = read_big_endian_u32(ptr);
        ptr += 4;
        left -= 4;

        if (ent.size == 0)
            break;

        if (left < 4)
            return false;
        ent.offset = read_big_endian_u32(ptr);
        ptr += 4;
        left -= 4;

        const auto *name_end = static_cast<const std::uint8_t *>(std::memchr(ptr, 0, left));
        if (!name_end)
            return false;
        const auto name_length = static_cast<std::size_t>(name_end - ptr);
        ent.name.assign(reinterpret_cast<const char *>(ptr), name_length);
        ptr = name_end + 1;
        left -= name_length + 1;

        entries_.push_back(std::move(ent));
    }

    content_offset_ = static_cast<std::size_t>(ptr - data_);
    return true;
}
