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
    if (!pl)
        throw std::runtime_error("cannot initialize player");
    player_.reset(pl);
}

void Player::play_midi(const std::uint8_t *msg, unsigned len)
{
    OPN2_MIDIPlayer *pl = player_.get();

    if (len == 0)
        return;

    const std::uint8_t status = msg[0];
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

void Player::generate(float *left, float *right, unsigned nframes, unsigned stride)
{
    // The library takes every sample format as a pointer to bytes and, for
    // this one, casts it back to float * to store each sample. So the buffers
    // go in as the floats they are, seen as bytes, which is allowed; a buffer
    // of bytes to copy out of afterwards would instead have the library store
    // floats in storage of another type, and without their alignment.
    const OPNMIDI_AudioFormat format {OPNMIDI_SampleType_F32, sizeof(float), static_cast<unsigned>(stride * sizeof(float))};
    opn2_generateFormat(player_.get(), static_cast<int>(2 * nframes),
                        reinterpret_cast<OPN2_UInt8 *>(left), reinterpret_cast<OPN2_UInt8 *>(right), &format);
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
