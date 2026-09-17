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

#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Reads the pack of instrument banks that tools/bankgen generates:
//
//   "PAK2"
//   for each bank: the size and the offset of its file, the size and the
//     offset of its text, as 32-bit big-endian numbers, then its name, ended
//     by a zero byte
//   a size of zero, which ends the list
//   one zlib stream, which holds the files and the texts at those offsets
class Pak_File_Reader {
public:
    bool init_with_data(const std::uint8_t *data, std::size_t size);
    std::size_t entry_count() const noexcept
        { return entries_.size(); }

    const std::string &name(std::size_t nth) const;
    // The bank file, in WOPL or WOPN format.
    std::vector<std::uint8_t> extract(std::size_t nth) const;
    // What the sources say of the bank, in UTF-8.
    std::string info(std::size_t nth) const;
    // The bank of the given name, if there is one.
    std::optional<std::size_t> find(std::string_view name) const;

private:
    struct Entry {
        std::uint32_t size = 0;
        std::uint32_t offset = 0;
        std::uint32_t info_size = 0;
        std::uint32_t info_offset = 0;
        std::string name;
    };

    const std::uint8_t *data_ = nullptr;
    std::size_t size_ = 0;

    std::vector<Entry> entries_;
    std::size_t content_offset_ = 0;

    bool read_dictionary();
    std::vector<std::uint8_t> read_content(std::uint32_t offset, std::uint32_t size) const;
};
