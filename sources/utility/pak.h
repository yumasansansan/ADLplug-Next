//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

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
