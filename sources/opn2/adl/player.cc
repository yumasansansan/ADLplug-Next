//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// SPDX-FileCopyrightText: 2018-2019 Jean Pierre Cimalando
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

#include "player.h"
#include <stdexcept>

void Player::init(unsigned sample_rate)
{
    OPN2_MIDIPlayer *pl = opn2_init(static_cast<long>(sample_rate));
    if (pl == nullptr)
        throw std::runtime_error("cannot initialize player");
    player_.reset(pl);
}

void Player::play_midi(const std::uint8_t *msg, unsigned len)
{
    OPN2_MIDIPlayer *pl = player_.get();

    if (len == 0)
        return;

    const std::uint8_t status = msg[0];
    // Only the channel messages: a System Exclusive message goes to
    // play_sysex(), and the rest of the system messages are not the player's.
    if ((status & 0xf0) == 0xf0)
        return;

    const auto channel = static_cast<OPN2_UInt8>(status & 0x0f);
    switch (status >> 4) {
    case 0b1001:
        if (len < 3)
            break;
        if (msg[2] != 0) {
            opn2_rt_noteOn(pl, channel, msg[1], msg[2]);
            break;
        }
        [[fallthrough]];  // a note-on with velocity 0 is a note-off
    case 0b1000:
        if (len < 3)
            break;
        opn2_rt_noteOff(pl, channel, msg[1]);
        break;
    case 0b1010:
        if (len < 3)
            break;
        opn2_rt_noteAfterTouch(pl, channel, msg[1], msg[2]);
        break;
    case 0b1101:
        if (len < 2)
            break;
        opn2_rt_channelAfterTouch(pl, channel, msg[1]);
        break;
    case 0b1011:
        if (len < 3)
            break;
        opn2_rt_controllerChange(pl, channel, msg[1], msg[2]);
        break;
    case 0b1100:
        if (len < 2)
            break;
        opn2_rt_patchChange(pl, channel, msg[1]);
        break;
    case 0b1110:
        if (len < 3)
            break;
        opn2_rt_pitchBendML(pl, channel, msg[2], msg[1]);
        break;
    default:
        break;
    }
}

bool Player::play_sysex(const std::uint8_t *msg, unsigned len)
{
    // The library wants the message whole, from its 0xf0 to its 0xf7.
    if (len < 2 || msg[0] != 0xf0 || msg[len - 1] != 0xf7)
        return false;
    return opn2_rt_systemExclusive(player_.get(), msg, len) > 0;
}

void Player::generate(double *left, double *right, unsigned nframes, unsigned stride)
{
    // **Binary64, and not the 32-bit float it was.** The library mixes the
    // chips into 32-bit integers and makes each one a double with a single
    // multiply, so from here on the sound is never held in single precision.
    //
    // Through pointers to the doubles they are: opn2_generateFormat takes every
    // sample format as the bytes of the buffers, so the buffers would have to be
    // handed over as another type than they are, and opn2_generateDouble, which
    // patches/libOPNMIDI/0018 adds, writes the same samples through pointers of
    // their own type.
    opn2_generateDouble(player_.get(), static_cast<int>(2 * nframes), left, right, stride);
}

std::vector<std::string> Player::enumerate_emulators()
{
    const std::unique_ptr<OPN2_MIDIPlayer, Player_Deleter> pl(opn2_init(44100));
    if (!pl)
        throw std::runtime_error("cannot initialize player");

    std::vector<std::string> names(32);
    std::size_t count = 0;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (opn2_switchEmulator(pl.get(), static_cast<int>(i)) == 0) {
            names[i] = opn2_chipEmulatorName(pl.get());
            count = i + 1;
        }
    }

    names.resize(count);
    return names;
}
