// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// Tests of the player, which is what the plugin gives its MIDI to.

#include "test.h"
#include "adl/instrument.h"
#include "adl/player.h"
#include "adl/wopx_file.h"
#include "resources.h"
#include "utility/pak.h"
#include "JuceHeader.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

namespace {

// The first instrument of the first melodic bank the plugin carries, which is
// something that sounds.
bool first_instrument(Instrument &ins)
{
    Pak_File_Reader pak;
    CHECK(pak.init_with_data(Res::banks_pak.data, Res::banks_pak.size));
    std::vector<std::uint8_t> data = pak.extract(0);
    const WOPx::BankFile_Ptr file(WOPx::LoadBankFromMem(data.data(), data.size(), nullptr));
    CHECK(file != nullptr);
    if (file == nullptr)
        return false;

    std::vector<Midi_Bank> banks;
    Instrument_Global_Parameters igp;
    Midi_Bank::from_wopl(*file, banks, igp);
    CHECK(!banks.empty());
    if (banks.empty())
        return false;

    ins = banks.at(0).ins[0];
    return true;
}

// A player with that instrument on program 0 of the first melodic bank.
void set_up(Player &pl, const Instrument &ins)
{
    pl.init(44100);
    Bank_Ref bank;
    pl.ensure_get_bank(Bank_Id(0, 0, false), Player::Bank_Create, bank);
    pl.ensure_set_instrument(bank, 0, ins);
}

// The energy of the next frames the player makes, a tenth of a second unless
// told otherwise.
double energy(Player &pl, unsigned frames = 4410)
{
    std::vector<float> left(frames), right(frames);
    pl.generate(left.data(), right.data(), frames, 1);

    double sum = 0;
    for (unsigned i = 0; i < frames; ++i) {
        const auto l = static_cast<double>(left[i]);
        const auto r = static_cast<double>(right[i]);
        sum += l * l + r * r;
    }
    return sum;
}

const std::uint8_t note_on[3] {0x90, 60, 127};
const std::uint8_t channel_volume_1[3] {0xb0, 7, 1};
// Below the default of 100, and loud enough to hear on either chip.
const std::uint8_t channel_volume_90[3] {0xb0, 7, 90};

// GM System On, for any device.
const std::uint8_t gm_system_on[6] {0xf0, 0x7e, 0x7f, 0x09, 0x01, 0xf7};

}  // namespace

// A System Exclusive message that resets the synthesizer puts the controllers
// of every channel back to where they start.
ADLPLUG_TEST(sysex_gm_reset)
{
    Instrument ins;
    if (!first_instrument(ins))
        return;

    Player plain;
    set_up(plain, ins);
    plain.play_midi(note_on, 3);
    const double loud = energy(plain);
    CHECK(loud > 0);

    Player quiet_player;
    set_up(quiet_player, ins);
    quiet_player.play_midi(channel_volume_1, 3);
    quiet_player.play_midi(note_on, 3);
    const double quiet = energy(quiet_player);
    CHECK(quiet < 0.5 * loud);

    // The reset gives the channel its volume back, and the note is as loud as
    // on a player that never had the controller changed.
    Player reset_player;
    set_up(reset_player, ins);
    reset_player.play_midi(channel_volume_1, 3);
    CHECK(reset_player.play_sysex(gm_system_on, sizeof gm_system_on));
    reset_player.play_midi(note_on, 3);
    CHECK(energy(reset_player) == loud);
}

// The way the player takes a channel of the chip for a new note: its own choice
// until it is told, every mode it knows, and its own choice again for a number
// that is no mode -- which is what the plugin counts on rather than checking the
// number itself (plugin_state.cc holds a project's number to the modes there are).
ADLPLUG_TEST(channel_allocation_mode)
{
    Player pl;
    pl.init(44100);
    CHECK(pl.channel_alloc_mode() == -1);

    // The three ways the libraries know: only a channel whose sounding delay has
    // expired, the first released channel with the same instrument, and the first
    // released channel of any.
    for (int mode = 0; mode <= 2; ++mode) {
        pl.set_channel_alloc_mode(mode);
        CHECK(pl.channel_alloc_mode() == mode);
    }

    pl.set_channel_alloc_mode(3);
    CHECK(pl.channel_alloc_mode() == -1);
    pl.set_channel_alloc_mode(-2);
    CHECK(pl.channel_alloc_mode() == -1);
}

// The master volume of a System Exclusive message is heard.
ADLPLUG_TEST(sysex_master_volume)
{
    Instrument ins;
    if (!first_instrument(ins))
        return;

    Player plain;
    set_up(plain, ins);
    plain.play_midi(note_on, 3);
    const double loud = energy(plain);
    CHECK(loud > 0);

    // Master volume, the fourteen bits least significant first: a quarter of
    // the way up.
    const std::uint8_t master_volume[8] {0xf0, 0x7f, 0x7f, 0x04, 0x01, 0x00, 0x20, 0xf7};
    Player softer;
    set_up(softer, ins);
    CHECK(softer.play_sysex(master_volume, sizeof master_volume));
    softer.play_midi(note_on, 3);
    const double soft = energy(softer);
    CHECK(soft > 0);
    CHECK(soft < 0.5 * loud);
}

// A System Exclusive message that cannot be used changes nothing: one that is
// not whole never reaches the library, and one for a maker the library does not
// know is turned away there.
ADLPLUG_TEST(sysex_needs_the_whole_message)
{
    Instrument ins;
    if (!first_instrument(ins))
        return;

    Player plain;
    set_up(plain, ins);
    plain.play_midi(channel_volume_90, 3);
    plain.play_midi(note_on, 3);
    const double turned_down = energy(plain);
    CHECK(turned_down > 0);

    const std::uint8_t start_only[1] {0xf0};
    const std::uint8_t without_the_end[5] {0xf0, 0x7e, 0x7f, 0x09, 0x01};
    const std::uint8_t without_the_start[5] {0x7e, 0x7f, 0x09, 0x01, 0xf7};
    // Whole, but for nobody the library knows.
    const std::uint8_t unknown_maker[4] {0xf0, 0x12, 0x00, 0xf7};

    Player player;
    set_up(player, ins);
    player.play_midi(channel_volume_90, 3);
    CHECK(!player.play_sysex(start_only, sizeof start_only));
    CHECK(!player.play_sysex(without_the_end, sizeof without_the_end));
    CHECK(!player.play_sysex(without_the_start, sizeof without_the_start));
    CHECK(!player.play_sysex(unknown_maker, sizeof unknown_maker));
    player.play_midi(note_on, 3);

    // Nothing was reset: the channel is still turned down where it was put.
    CHECK(energy(player) == turned_down);
}

// A player plays on one thread while another thread makes players, for each
// emulator a build has. What an emulator's chips share -- tables worked out from
// constants, or anything else kept once for every chip -- a chip made after the
// first must leave alone, because a chip already playing on another thread is
// reading it. That is what the plugin does -- its worker makes chips of its own
// to measure an instrument while the audio thread plays -- and what two instances
// of the plugin in one host do. It is the thread sanitizer that sees a chip that
// writes it; without it, the test sees only that every player played.
ADLPLUG_TEST(players_made_while_another_plays)
{
    Instrument ins;
    if (!first_instrument(ins))
        return;

    const std::vector<std::string> emulators = Player::enumerate_emulators();
    CHECK(!emulators.empty());
    for (unsigned emu = 0; emu < emulators.size(); ++emu) {
        // The number of an emulator the build leaves out, between two it has.
        if (emulators[emu].empty())
            continue;

        Player playing;
        set_up(playing, ins);
        playing.set_emulator(emu);
        CHECK(playing.emulator() == emu);
        playing.play_midi(note_on, 3);

        // Short blocks until the note is heard, by when the chip has read what it
        // shares: the low-level emulators are slow, and slower still under a
        // sanitizer.
        double heard = 0;
        std::thread audio([&playing, &heard] {
            for (unsigned block = 0; block < 400 && heard == 0; ++block)
                heard = energy(playing, 256);
        });
        for (unsigned made = 0; made < 3; ++made) {
            Player another;
            another.init(44100);
            another.set_emulator(emu);
        }
        audio.join();
        CHECK(heard > 0);
    }
}
