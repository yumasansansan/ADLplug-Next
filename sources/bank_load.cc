// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

#include "bank_load.h"
#include "adl/wopx_file.h"

std::optional<Bank_File_Contents> read_bank_file(std::span<std::uint8_t> data)
{
    // The library reads the bytes and says nothing but whether they were a bank:
    // a file of the wrong format, or one that ends in the middle of what it
    // promised, comes back as nothing at all.
    const WOPx::BankFile_Ptr file(WOPx::LoadBankFromMem(data.data(), data.size(), nullptr));
    if (!file)
        return {};

    Bank_File_Contents contents;
    Midi_Bank::from_wopl(*file, contents.banks, contents.global);
    // A bank file of this format carries how long every instrument sounds, so the
    // worker has nothing left to measure.
    contents.need_measurement = false;
#if defined(ADLPLUG_OPN2)
    contents.chip_type = int{file->chip_type};
#endif
    return contents;
}

std::optional<Instrument> read_instrument_file(std::span<std::uint8_t> data, int format)
{
    switch (format) {
    default: {
        WOPx::InstrumentFile file = {};
        if (WOPx::LoadInstFromMem(&file, data.data(), data.size()) != 0)
            return {};
        return Instrument::from_wopl(file.inst);
    }
#if defined(ADLPLUG_OPL3)
    case 1: {
        // SBI says nothing about whether it was read: an instrument that came out
        // blank is what it answers with, and that is no instrument to load.
        const Instrument instrument = Instrument::from_sbi(data.data(), data.size());
        if (instrument.blank())
            return {};
        return instrument;
    }
#endif
    }
}
