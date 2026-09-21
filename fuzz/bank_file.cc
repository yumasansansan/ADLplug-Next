// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// The bank and instrument files that people open, which come from anywhere.
// The input goes where the editor sends a file
// (Generic_Main_Component::load_bank_mem and load_single_instrument_mem):
//
//  - as a bank (WOPL or WOPN): loaded, it is saved again in its own version
//    and loaded back, and must be the same, but for the marks of blank
//    instruments in a WOPN bank of version 1, which has no place for them
//    either: loading makes up a bank of blank instruments where a file has
//    none, and saving it in version 1 cannot tell them apart from the others;
//    each of its instruments converts
//    to ADLplug-Next's own form and back unchanged, and saved as an instrument
//    file and loaded back, is the same but for what instrument files cannot
//    hold: the sounding delays, and for OPN2 the mark of a blank instrument,
//    which a WOPN bank of version 2 writes as delays of zero and an OPNI file
//    has no place for;
//  - as an instrument file (OPLI or OPNI): loaded, saved again in its own
//    version and loaded back, it is the same, the version included, and it
//    converts to ADLplug-Next's form and back unchanged;
//  - as an SBI instrument, for OPL3.
//
// Whatever loads has a version that the format defines: WOPL 1 to 3, WOPN 1 to
// 2. The version code of a file is an unsigned 16-bit number, so what would be
// a negative version is one far too new, and a loader must turn it away as it
// must 0. When a library learns a new version, this is where to say so.
//
// Each conversion is checked from what the first one gave, not from the input:
// loading may settle a value that the input leaves open, and what matters is
// that nothing changes after that.

#include "fuzz.h"
#include "adl/instrument.h"
#include "adl/wopx_file.h"
#include <vector>

namespace {

#if defined(ADLPLUG_OPL3)
constexpr std::uint16_t latest_version = 3;
#elif defined(ADLPLUG_OPN2)
constexpr std::uint16_t latest_version = 2;
#endif

void check_instrument_conversion(const WOPx::Instrument &from_file)
{
    const Instrument ins = Instrument::from_wopl(from_file);
    const Instrument again = Instrument::from_wopl(ins.to_wopl());
    FUZZ_CHECK(ins.equal_instrument(again));
}

// Saves the instrument file in the given version (0 for the latest) and loads
// it back.
WOPx::InstrumentFile saved_and_loaded(WOPx::InstrumentFile file, std::uint16_t version)
{
    std::vector<char> saved(WOPx::CalculateInstFileSize(&file, version));
    FUZZ_CHECK(WOPx::SaveInstToMem(&file, saved.data(), saved.size(), version) == 0);

    WOPx::InstrumentFile reloaded {};
    FUZZ_CHECK(WOPx::LoadInstFromMem(&reloaded, saved.data(), saved.size()) == 0);
    FUZZ_CHECK(reloaded.is_drum == file.is_drum);
    return reloaded;
}

// The instrument as the comparison wants it. An instrument file has no place
// for the mark of a blank instrument, so for OPN2 the mark is cleared on both
// sides before they are compared; the note at the top of this file says what
// each format holds. The mark is cleared where the instrument is made, so that
// what is compared is settled once and does not change afterwards.
Instrument instrument_to_compare(const WOPx::Instrument &wopl)
{
    Instrument ins = Instrument::from_wopl(wopl);
#if defined(ADLPLUG_OPN2)
    ins.blank(false);
#endif
    return ins;
}

void check_as_instrument_file(const WOPx::Instrument &inst, bool is_drum)
{
    WOPx::InstrumentFile file {};
    file.is_drum = is_drum ? 1 : 0;
    file.inst = inst;
    const WOPx::InstrumentFile reloaded = saved_and_loaded(file, 0);

    const Instrument before = instrument_to_compare(file.inst);
    const Instrument after = instrument_to_compare(reloaded.inst);
    FUZZ_CHECK(before.equal_instrument_except_delays(after));
}

#if defined(ADLPLUG_OPN2)
// The instruments are reached through the pointers the file holds, so the
// reference itself is never written through and the check offers to make it const.
// Clearing the marks is what this function is for; a file it says it does not
// change is not what it is given.
// NOLINTNEXTLINE(misc-const-correctness)
void clear_blank_marks(WOPx::BankFile &file)
{
    for (unsigned b = 0; b < file.banks_count_melodic; ++b) {
        for (WOPx::Instrument &inst : file.banks_melodic[b].ins)
            inst.inst_flags &= static_cast<std::uint8_t>(~WOPx::Ins_IsBlank);
    }
    for (unsigned b = 0; b < file.banks_count_percussion; ++b) {
        for (WOPx::Instrument &inst : file.banks_percussive[b].ins)
            inst.inst_flags &= static_cast<std::uint8_t>(~WOPx::Ins_IsBlank);
    }
}
#endif

void fuzz_bank(std::vector<std::uint8_t> input)
{
    const WOPx::BankFile_Ptr file(WOPx::LoadBankFromMem(input.data(), input.size(), nullptr));
    if (!file)
        return;
    FUZZ_CHECK(file->version >= 1 && file->version <= latest_version);

    std::vector<Midi_Bank> banks;
    Instrument_Global_Parameters igp;
    Midi_Bank::from_wopl(*file, banks, igp);
    FUZZ_CHECK(banks.size() ==
               std::size_t{file->banks_count_melodic} + std::size_t{file->banks_count_percussion});

    for (unsigned b = 0; b < file->banks_count_melodic; ++b) {
        for (const WOPx::Instrument &inst : file->banks_melodic[b].ins) {
            check_instrument_conversion(inst);
            check_as_instrument_file(inst, false);
        }
    }
    for (unsigned b = 0; b < file->banks_count_percussion; ++b) {
        for (const WOPx::Instrument &inst : file->banks_percussive[b].ins) {
            check_instrument_conversion(inst);
            check_as_instrument_file(inst, true);
        }
    }

    // Last, since it may clear the marks of the instruments checked above.
    std::vector<char> saved(WOPx::CalculateBankFileSize(file.get(), file->version));
    FUZZ_CHECK(WOPx::SaveBankToMem(file.get(), saved.data(), saved.size(), file->version, 0) == 0);
    const WOPx::BankFile_Ptr reloaded(WOPx::LoadBankFromMem(saved.data(), saved.size(), nullptr));
    FUZZ_CHECK(reloaded != nullptr);
#if defined(ADLPLUG_OPN2)
    if (file->version < 2) {
        clear_blank_marks(*file);
        clear_blank_marks(*reloaded);
    }
#endif
    FUZZ_CHECK(WOPx::BanksCmp(file.get(), reloaded.get()) == 1);
}

void fuzz_instrument(std::vector<std::uint8_t> input)
{
    WOPx::InstrumentFile file {};
    if (WOPx::LoadInstFromMem(&file, input.data(), input.size()) != 0)
        return;
    FUZZ_CHECK(file.version >= 1 && file.version <= latest_version);

    const WOPx::InstrumentFile reloaded = saved_and_loaded(file, file.version);
    FUZZ_CHECK(reloaded.version == file.version);

    check_instrument_conversion(file.inst);
    check_as_instrument_file(file.inst, file.is_drum != 0);
}

#if defined(ADLPLUG_OPL3)
void fuzz_sbi(const std::vector<std::uint8_t> &input)
{
    const Instrument ins = Instrument::from_sbi(input.data(), input.size());
    if (ins.blank())
        return;

    const Instrument again = Instrument::from_wopl(ins.to_wopl());
    FUZZ_CHECK(ins.equal_instrument(again));
}
#endif

// Nothing at all, which is as much an input as bytes are: the loaders are handed
// no memory, once with a size that says so and once with a size that says there
// are bytes to read. A file dialogue can give a plugin an empty file, and a host
// can hand over a pointer to nothing.
void fuzz_nothing(std::size_t size)
{
    const WOPx::BankFile_Ptr none(WOPx::LoadBankFromMem(nullptr, 0, nullptr));
    FUZZ_CHECK(!none);
    const WOPx::BankFile_Ptr lying(WOPx::LoadBankFromMem(nullptr, size, nullptr));
    FUZZ_CHECK(!lying);

    WOPx::InstrumentFile file {};
    FUZZ_CHECK(WOPx::LoadInstFromMem(&file, nullptr, 0) != 0);
    FUZZ_CHECK(WOPx::LoadInstFromMem(&file, nullptr, size) != 0);

#if defined(ADLPLUG_OPL3)
    FUZZ_CHECK(Instrument::from_sbi(nullptr, 0).blank());
    FUZZ_CHECK(Instrument::from_sbi(nullptr, size).blank());
#endif
}

}  // namespace

int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    // The loaders take their memory as writable. A copy of exactly the input's
    // size keeps a read past its end in view of the address sanitizer, and
    // libFuzzer's own buffer out of reach of any write.
    const std::vector<std::uint8_t> input(data, data + size);

    fuzz_nothing(size);
    fuzz_bank(input);
    fuzz_instrument(input);
#if defined(ADLPLUG_OPL3)
    fuzz_sbi(input);
#endif
    return 0;
}
