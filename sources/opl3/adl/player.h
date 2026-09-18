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

#pragma once
#include "instrument.h"
#include <adlmidi.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Player {
public:
    static constexpr int Bank_Create = ADLMIDI_Bank_Create;
    static constexpr int Bank_CreateRt = ADLMIDI_Bank_CreateRt;

    void init(unsigned sample_rate);
    void close()
        { player_.reset(); }

    static const char *name() noexcept
        { return "ADLMIDI"; }
    static double output_gain()
        { return std::pow(10.0, 3.0 / 20.0); }
    static std::vector<std::string> enumerate_emulators();

    void reset()
        { adl_reset(player_.get()); }
    void panic()
        { adl_panic(player_.get()); }
    bool reserve_banks(unsigned banks)
        { return adl_reserveBanks(player_.get(), banks) >= 0; }
    bool load_bank_data(const void *mem, std::size_t size)
        { return adl_openBankData(player_.get(), mem, static_cast<unsigned long>(size)) >= 0; }
    bool get_bank(const Bank_Id &id, int flags, Bank_Ref &bank)
        { return adl_getBank(player_.get(), &id, flags, &bank) >= 0; }
    bool get_first_bank(Bank_Ref &bank)
        { return adl_getFirstBank(player_.get(), &bank) >= 0; }
    bool get_next_bank(Bank_Ref &bank)
        { return adl_getNextBank(player_.get(), &bank) >= 0; }
    bool get_bank_id(const Bank_Ref &bank, Bank_Id &id)
        { return adl_getBankId(player_.get(), &bank, &id) >= 0; }
    bool remove_bank(Bank_Ref &bank)
        { return adl_removeBank(player_.get(), &bank) >= 0; }
    bool get_instrument(const Bank_Ref &bank, unsigned index, Instrument &ins)
        { return adl_getInstrument(player_.get(), &bank, index, &ins) >= 0; }
    bool set_instrument(Bank_Ref &bank, unsigned index, const Instrument &ins)
        { return adl_setInstrument(player_.get(), &bank, index, &ins) >= 0; }
    const char *emulator_name() const
        { return adl_chipEmulatorName(player_.get()); }
    unsigned emulator() const noexcept
        { return emu_; }
    void set_emulator(unsigned emu)
        { if (adl_switchEmulator(player_.get(), static_cast<int>(emu)) >= 0) emu_ = emu; }
    unsigned num_chips() const
        { return static_cast<unsigned>(std::max(0, adl_getNumChipsObtained(player_.get()))); }
    bool set_num_chips(unsigned chips)
        { return adl_setNumChips(player_.get(), static_cast<int>(chips)) == 0; }
    unsigned num_4ops() const
        { return static_cast<unsigned>(std::max(0, adl_getNumFourOpsChnObtained(player_.get()))); }
    bool set_num_4ops(unsigned count);
    int volume_model() const
        { return adl_getVolumeRangeModel(player_.get()); }
    void set_volume_model(int model)
        { adl_setVolumeRangeModel(player_.get(), model); }
    bool deep_tremolo() const
        { return adl_getHTremolo(player_.get()) != 0; }
    void set_deep_tremolo(bool trem)
        { adl_setHTremolo(player_.get(), trem ? 1 : 0); }
    bool deep_vibrato() const
        { return adl_getHVibrato(player_.get()) != 0; }
    void set_deep_vibrato(bool vib)
        { adl_setHVibrato(player_.get(), vib ? 1 : 0); }
    bool mt32_defaults() const;
    void set_mt32_defaults(bool mt32);
    void set_soft_pan_enabled(bool sp)
        { adl_setSoftPanEnabled(player_.get(), sp ? 1 : 0); }
    void play_midi(const std::uint8_t *msg, unsigned len);
    // A whole System Exclusive message, 0xf0 to 0xf7, as a host hands one over.
    // The library reads out of it the resets of GM, GS and XG, the master
    // volume, and which channels GS makes percussive. True when it acted on the
    // message, which for a reset means that every note has stopped.
    bool play_sysex(const std::uint8_t *msg, unsigned len);
    void generate(float *left, float *right, unsigned nframes, unsigned stride);

    void ensure_get_bank_id(const Bank_Ref &bank, Bank_Id &id)
        { [[maybe_unused]] const bool success = get_bank_id(bank, id); assert(success); }
    void ensure_get_bank(const Bank_Id &id, int flags, Bank_Ref &bank)
        { [[maybe_unused]] const bool success = get_bank(id, flags, bank); assert(success); }
    void ensure_remove_bank(Bank_Ref &bank)
        { [[maybe_unused]] const bool success = remove_bank(bank); assert(success); }
    void ensure_get_instrument(const Bank_Ref &bank, unsigned index, Instrument &ins)
        { [[maybe_unused]] const bool success = get_instrument(bank, index, ins); assert(success); }
    void ensure_set_instrument(Bank_Ref &bank, unsigned index, const Instrument &ins)
        { [[maybe_unused]] const bool success = set_instrument(bank, index, ins); assert(success); }

private:
    struct Player_Deleter {
        void operator()(ADL_MIDIPlayer *p) const noexcept { adl_close(p); }
    };
    unsigned emu_ = 0;
    std::unique_ptr<ADL_MIDIPlayer, Player_Deleter> player_;
};
