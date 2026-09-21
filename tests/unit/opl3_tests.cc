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
// Two of the library's emulator cores, to play a rhythm-mode drum on directly:
// there is no bank of one here, and the registers say it in five writes. The
// DOSBox core is in every build (cmake/ADLMIDI.cmake), and the Nuked one is
// there when the build has it (tests/CMakeLists.txt).
#include "chips/dosbox_opl3.h"
#if defined(ADLPLUG_TESTS_HAVE_NUKED_OPL3)
#include "chips/nuked_opl3.h"
#endif
#include "JuceHeader.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
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

namespace {

struct Peaks { int left; int right; };

// Keys the given rhythm-mode drum on, with the panning bits of every rhythm
// channel set to `bits` and the soft panning of the library to `soft_pan`, and
// returns the peak of each output.
Peaks rhythm_peaks(OPLChipBase &chip, unsigned key, unsigned bits, unsigned soft_pan)
{
    chip.setRate(44100);
    chip.reset();
    chip.writeReg(0x105, 0x01);  // OPL3 mode, which is where the panning is
    chip.writeReg(0x01, 0x20);
    // The operators of channels 6, 7 and 8, which are the ones rhythm mode plays.
    for (const unsigned op : {0x12u, 0x15u, 0x13u, 0x16u, 0x14u, 0x17u}) {
        chip.writeReg(static_cast<std::uint16_t>(0x20 + op), 0x21);  // sustaining, multiple one
        chip.writeReg(static_cast<std::uint16_t>(0x40 + op), 0x00);  // full level
        chip.writeReg(static_cast<std::uint16_t>(0x60 + op), 0xf0);  // fastest attack, slowest decay
        chip.writeReg(static_cast<std::uint16_t>(0x80 + op), 0x00);  // full sustain
        chip.writeReg(static_cast<std::uint16_t>(0xe0 + op), 0x00);  // sine
    }
    for (unsigned channel = 6; channel <= 8; ++channel) {
        chip.writeReg(static_cast<std::uint16_t>(0xa0 + channel), 0x40);
        chip.writeReg(static_cast<std::uint16_t>(0xb0 + channel), 0x0d);  // block three, no key on
        chip.writeReg(static_cast<std::uint16_t>(0xc0 + channel), static_cast<std::uint8_t>(bits));
        chip.writePan(static_cast<std::uint16_t>(0xc0 + channel), static_cast<std::uint8_t>(soft_pan));
    }
    chip.writeReg(0xbd, 0x20);  // rhythm mode, and then the drum keyed on: a
    chip.writeReg(0xbd, static_cast<std::uint8_t>(0x20 | key));  // rhythm key is here, not in 0xb0

    constexpr std::size_t frames = 2048;
    std::vector<std::int16_t> out(2 * frames, 0);
    chip.generate(out.data(), frames);

    Peaks peaks {.left = 0, .right = 0};
    for (std::size_t i = 0; i < frames; ++i) {
        peaks.left = std::max(peaks.left, std::abs(static_cast<int>(out[2 * i])));
        peaks.right = std::max(peaks.right, std::abs(static_cast<int>(out[2 * i + 1])));
    }
    return peaks;
}

void check_rhythm_panning(OPLChipBase &chip)
{
    // The bass drum is made by the operators of channel 6, the snare drum by one
    // of channel 7's, and the tom-tom by one of channel 8's.
    for (const unsigned key : {0x10u, 0x08u, 0x04u}) {
        const Peaks both = rhythm_peaks(chip, key, 0x30, 64);
        const Peaks left_bit = rhythm_peaks(chip, key, 0x10, 64);
        const Peaks hard_left = rhythm_peaks(chip, key, 0x30, 0);

        // Heard from both outputs, equally, when both bits say so.
        CHECK(both.left > 0);
        CHECK(both.left == both.right);
        // The right output silent when the right bit is clear, the left one as
        // loud as it was.
        CHECK(left_bit.left == both.left);
        CHECK(left_bit.right == 0);
        // Panning hard left is the library's own, and it takes the whole of the
        // sound to the left output: the pan law takes a part of it away at centre.
        CHECK(hard_left.right == 0);
        CHECK(hard_left.left > both.left);
    }
}

}  // namespace

ADLPLUG_TEST(rhythm_mode_drums_are_panned)
{
    // A drum of rhythm mode is heard from the output its channel says, as every
    // other sound of the chip is. The DOSBox core summed the five drums into both
    // outputs whatever their channels said, which is what
    // patches/libADLMIDI/0014-dosbox-a-rhythm-mode-drum-is-panned-by-its-own-channel.patch
    // fixes; the Nuked core, written from a die scan of the chip, is here as what
    // the chip does.
    DosBoxOPL3 dosbox;
    check_rhythm_panning(dosbox);
#if defined(ADLPLUG_TESTS_HAVE_NUKED_OPL3)
    NukedOPL3 nuked;
    check_rhythm_panning(nuked);
#endif
}
