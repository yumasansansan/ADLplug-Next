// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

#pragma once
#include <cstdint>

// A four-character code as a 32-bit value, first character in the most
// significant byte: fourcc("chip") == 0x63686970. That is the value GCC,
// Clang and MSVC give the multi-character literal 'chip', but multi-character
// literals are implementation-defined and draw a warning from GCC, so tags are
// spelled with this instead.
constexpr std::uint32_t fourcc(const char (&code)[5]) noexcept
{
    return static_cast<std::uint32_t>(static_cast<unsigned char>(code[0])) << 24 |
           static_cast<std::uint32_t>(static_cast<unsigned char>(code[1])) << 16 |
           static_cast<std::uint32_t>(static_cast<unsigned char>(code[2])) << 8 |
           static_cast<std::uint32_t>(static_cast<unsigned char>(code[3]));
}
