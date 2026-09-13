//     Part of ADLplug, distributed under the GNU GPL v3 or later.
//               (See accompanying file LICENSE.)

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
