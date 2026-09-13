//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

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
