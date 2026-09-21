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
#include <cstddef>
#include <stdexcept>

// From libADLMIDI's source tree, for the MT-32 defaults. The definitions that
// shape these classes, such as ADLMIDI_DISABLE_MIDI_SEQUENCER, are public ones
// of ADLMIDI_static, so this file sees the classes as the library does.
#include "adlmidi_midiplay.hpp"
#include "adlmidi_opl3.hpp"

void Player::init(unsigned sample_rate)
{
    ADL_MIDIPlayer *pl = adl_init(static_cast<long>(sample_rate));
    if (pl == nullptr)
        throw std::runtime_error("cannot initialize player");
    player_.reset(pl);
}

bool Player::set_num_4ops(unsigned count)
{
    ADL_MIDIPlayer *pl = player_.get();
    int ops4 = static_cast<int>(count);
    if (count == ~0u) {
        // set automatic count
        if (adl_setNumFourOpsChn(pl, -1) < 0)
            return false;
        // get the fixed count and set it, so it doesn't remain automatic
        ops4 = adl_getNumFourOpsChnObtained(pl);
    }
    return adl_setNumFourOpsChn(pl, ops4) >= 0;
}

// libADLMIDI has no function for the MT-32 defaults: only a WOPL bank opened as
// a whole sets them (adl_openBankData()), and ADLplug gives the player its
// instruments one by one. So these reach the state that opening such a bank
// sets. Besides the flag, that is the defaults which each MIDI channel takes
// when it is reset (MIDIplay::resetMIDIDefaults(), which a bank without the
// flag leaves as they are, so they are set back here). A change also puts the
// channels' volume and pitch bend range to the new defaults, as the start of a
// song would.
bool Player::mt32_defaults() const
{
    const MIDIplay &play = *static_cast<const MIDIplay *>(player_->adl_midiPlayer);
    return play.m_synth->m_insBankSetup.mt32defaults;
}

void Player::set_mt32_defaults(bool mt32)
{
    ADL_MIDIPlayer *pl = player_.get();
    MIDIplay &play = *static_cast<MIDIplay *>(pl->adl_midiPlayer);
    Synth &synth = *play.m_synth;
    if (synth.m_insBankSetup.mt32defaults == mt32)
        return;
    synth.m_insBankSetup.mt32defaults = mt32;

    const bool wide = mt32 || synth.m_musicMode == Synth::MODE_RSXX;
    for (std::size_t c = 0; c < play.m_midiChannels.size; ++c) {
        MIDIplay::MIDIchannel &ch = play.m_midiChannels[c];
        ch.def_volume = wide ? 127 : 100;
        ch.def_bendsense_lsb = 0;
        ch.def_bendsense_msb = wide ? 12 : 2;
        ch.bendsense_lsb = ch.def_bendsense_lsb;
        ch.bendsense_msb = ch.def_bendsense_msb;
        ch.updateBendSensitivity();
        if (c < 16)
            adl_rt_controllerChange(pl, static_cast<ADL_UInt8>(c), 7, ch.def_volume);
    }
}

void Player::play_midi(const std::uint8_t *msg, unsigned len)
{
    ADL_MIDIPlayer *pl = player_.get();

    if (len == 0)
        return;

    const std::uint8_t status = msg[0];
    // Only the channel messages: a System Exclusive message goes to
    // play_sysex(), and the rest of the system messages are not the player's.
    if ((status & 0xf0) == 0xf0)
        return;

    const auto channel = static_cast<ADL_UInt8>(status & 0x0f);
    switch (status >> 4) {
    case 0b1001:
        if (len < 3)
            break;
        if (msg[2] != 0) {
            adl_rt_noteOn(pl, channel, msg[1], msg[2]);
            break;
        }
        [[fallthrough]];  // a note-on with velocity 0 is a note-off
    case 0b1000:
        if (len < 3)
            break;
        adl_rt_noteOff(pl, channel, msg[1]);
        break;
    case 0b1010:
        if (len < 3)
            break;
        adl_rt_noteAfterTouch(pl, channel, msg[1], msg[2]);
        break;
    case 0b1101:
        if (len < 2)
            break;
        adl_rt_channelAfterTouch(pl, channel, msg[1]);
        break;
    case 0b1011:
        if (len < 3)
            break;
        adl_rt_controllerChange(pl, channel, msg[1], msg[2]);
        break;
    case 0b1100:
        if (len < 2)
            break;
        adl_rt_patchChange(pl, channel, msg[1]);
        break;
    case 0b1110:
        if (len < 3)
            break;
        adl_rt_pitchBendML(pl, channel, msg[2], msg[1]);
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
    return adl_rt_systemExclusive(player_.get(), msg, len) > 0;
}

void Player::generate(float *left, float *right, unsigned nframes, unsigned stride)
{
    // The library takes every sample format as a pointer to bytes and, for
    // this one, casts it back to float * to store each sample. So the buffers
    // go in as the floats they are, seen as bytes, which is allowed; a buffer
    // of bytes to copy out of afterwards would instead have the library store
    // floats in storage of another type, and without their alignment.
    const ADLMIDI_AudioFormat format {ADLMIDI_SampleType_F32, sizeof(float), static_cast<unsigned>(stride * sizeof(float))};
    // The library takes the bytes of the buffers, and the format of a sample
    // beside them, which is how it writes floats into them.
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
    adl_generateFormat(player_.get(), static_cast<int>(2 * nframes),
                       reinterpret_cast<ADL_UInt8 *>(left), reinterpret_cast<ADL_UInt8 *>(right), &format);
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
}

std::vector<std::string> Player::enumerate_emulators()
{
    const std::unique_ptr<ADL_MIDIPlayer, Player_Deleter> pl(adl_init(44100));
    if (!pl)
        throw std::runtime_error("cannot initialize player");

    std::vector<std::string> names(32);
    std::size_t count = 0;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (adl_switchEmulator(pl.get(), static_cast<int>(i)) == 0) {
            names[i] = adl_chipEmulatorName(pl.get());
            count = i + 1;
        }
    }

    names.resize(count);
    return names;
}
