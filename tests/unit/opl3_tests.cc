// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// Tests of the OPL3 build only.

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
#include <vector>

namespace {

// Plays middle C for half a second with the given instrument on program 0 of
// the first melodic bank, and returns the energy of the output. When `from` is
// given, the player has those MT-32 defaults first and changes to `mt32`.
double note_energy(const Instrument &ins, bool mt32, int from = -1)
{
    Player pl;
    pl.init(44100);
    Bank_Ref bank;
    pl.ensure_get_bank(Bank_Id(0, 0, false), Player::Bank_Create, bank);
    pl.ensure_set_instrument(bank, 0, ins);

    if (from >= 0)
        pl.set_mt32_defaults(from != 0);
    pl.set_mt32_defaults(mt32);
    CHECK(pl.mt32_defaults() == mt32);

    const std::uint8_t note_on[3] {0x90, 60, 127};
    pl.play_midi(note_on, 3);

    constexpr unsigned frames = 22050;
    std::vector<float> left(frames), right(frames);
    pl.generate(left.data(), right.data(), frames, 1);

    double energy = 0;
    for (std::size_t i = 0; i < frames; ++i) {
        const auto l = static_cast<double>(left[i]);
        const auto r = static_cast<double>(right[i]);
        energy += l * l + r * r;
    }
    return energy;
}

}  // namespace

ADLPLUG_TEST(mt32_defaults)
{
    Pak_File_Reader pak;
    CHECK(pak.init_with_data(Res::banks_pak.data, Res::banks_pak.size));
    std::vector<std::uint8_t> data = pak.extract(0);
    const WOPx::BankFile_Ptr file(WOPx::LoadBankFromMem(data.data(), data.size(), nullptr));
    CHECK(file != nullptr);
    if (file == nullptr)
        return;

    std::vector<Midi_Bank> banks;
    Instrument_Global_Parameters igp;
    Midi_Bank::from_wopl(*file, banks, igp);
    const Instrument &ins = banks.at(0).ins[0];

    // With the MT-32 defaults, a channel starts at volume 127 instead of 100,
    // so the same note is louder.
    const double generic = note_energy(ins, false);
    const double mt32 = note_energy(ins, true);
    CHECK(generic > 0);
    CHECK(mt32 > 1.2 * generic);

    // Changed back, the player has the generic defaults again, and plays the
    // note exactly as one that never had the MT-32 ones.
    CHECK(note_energy(ins, false, 1) == generic);
    CHECK(note_energy(ins, true, 0) == mt32);
}

ADLPLUG_TEST(mt32_defaults_in_state)
{
    Instrument_Global_Parameters gp;
    gp.mt32_defaults = true;
    CHECK(Instrument_Global_Parameters::from_properties(gp.to_properties()) == gp);

    // A state saved before the flag existed has the generic defaults.
    PropertySet old = gp.to_properties();
    old.removeValue("mt32_defaults");
    CHECK(!Instrument_Global_Parameters::from_properties(old).mt32_defaults);
}
