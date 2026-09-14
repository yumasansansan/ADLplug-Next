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
#include <string>
#include <vector>

// Reads the bank archive: a dictionary of (size, offset, name) entries followed
// by one gzip stream that holds all the files.
class Pak_File_Reader {
public:
    bool init_with_data(const std::uint8_t *data, std::size_t size);
    std::size_t entry_count() const noexcept
        { return entries_.size(); }

    const std::string &name(std::size_t nth) const;
    std::string extract(std::size_t nth) const;

private:
    struct Entry {
        std::uint32_t size = 0;
        std::uint32_t offset = 0;
        std::string name;
    };

    const std::uint8_t *data_ = nullptr;
    std::size_t size_ = 0;

    std::vector<Entry> entries_;
    std::size_t content_offset_ = 0;

    bool read_dictionary();
};
