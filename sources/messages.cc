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

#include "messages.h"

namespace Messages {

Buffered_Message read(Simple_Fifo &fifo) noexcept
{
    Buffered_Message msg;
    unsigned offset = 0;
    const std::uint8_t *header = fifo.read(sizeof msg.header, offset);
    if (header == nullptr)
        return {};
    std::memcpy(&msg.header, header, sizeof msg.header);
    std::uint8_t *body = fifo.read(msg.header.size, offset);
    if (body == nullptr)
        return {};
    msg.body = {body, msg.header.size};
    msg.length = offset;
    msg.valid = true;
    return msg;
}

void finish_read(Simple_Fifo &fifo, const Buffered_Message &msg) noexcept
{
    fifo.finish_read(msg.length);
}

Buffered_Message write(Simple_Fifo &fifo, unsigned tag, unsigned size) noexcept
{
    Buffered_Message msg;
    msg.header = Message_Header{tag, size};
    unsigned offset = 0;
    std::uint8_t *header = fifo.write(sizeof msg.header, offset);
    if (header == nullptr)
        return {};
    std::memcpy(header, &msg.header, sizeof msg.header);
    std::uint8_t *body = fifo.write(size, offset);
    if (body == nullptr)
        return {};
    msg.body = {body, size};
    msg.length = offset;
    msg.valid = true;
    return msg;
}

void finish_write(Simple_Fifo &fifo, const Buffered_Message &msg) noexcept
{
    fifo.finish_write(msg.length);
}

}  // namespace Messages
