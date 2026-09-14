// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

#include "test.h"
#include "adl/instrument.h"
#include "adl/wopx_file.h"
#include "resources.h"
#include "utility/pak.h"
#include "JuceHeader.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <vector>

namespace {

// Prints the fields of the file format in which two instruments differ.
void print_differences(const std::string &bank, const char *conversion, unsigned program,
                       const WOPx::Instrument &before, const WOPx::Instrument &after)
{
    const auto field = [&](const char *name, int x, int y) {
        if (x != y)
            std::fprintf(stderr, "  %s, program %u, through the %s: %s %d (0x%02x) became %d (0x%02x)\n",
                         bank.c_str(), program, conversion, name,
                         x, static_cast<unsigned>(x) & 0xffffu, y, static_cast<unsigned>(y) & 0xffffu);
    };
#if defined(ADLPLUG_OPL3)
    field("note_offset1", before.note_offset1, after.note_offset1);
    field("note_offset2", before.note_offset2, after.note_offset2);
    field("midi_velocity_offset", before.midi_velocity_offset, after.midi_velocity_offset);
    field("second_voice_detune", before.second_voice_detune, after.second_voice_detune);
    field("percussion_key_number", before.percussion_key_number, after.percussion_key_number);
    field("inst_flags", before.inst_flags, after.inst_flags);
    field("fb_conn1_C0", before.fb_conn1_C0, after.fb_conn1_C0);
    field("fb_conn2_C0", before.fb_conn2_C0, after.fb_conn2_C0);
    for (unsigned op = 0; op < 4; ++op) {
        const WOPLOperator &b = before.operators[op];
        const WOPLOperator &a = after.operators[op];
        field("avekf_20", b.avekf_20, a.avekf_20);
        field("ksl_l_40", b.ksl_l_40, a.ksl_l_40);
        field("atdec_60", b.atdec_60, a.atdec_60);
        field("susrel_80", b.susrel_80, a.susrel_80);
        field("waveform_E0", b.waveform_E0, a.waveform_E0);
    }
#elif defined(ADLPLUG_OPN2)
    field("note_offset", before.note_offset, after.note_offset);
    field("midi_velocity_offset", before.midi_velocity_offset, after.midi_velocity_offset);
    field("percussion_key_number", before.percussion_key_number, after.percussion_key_number);
    field("inst_flags", before.inst_flags, after.inst_flags);
    field("fbalg", before.fbalg, after.fbalg);
    field("lfosens", before.lfosens, after.lfosens);
    for (unsigned op = 0; op < 4; ++op) {
        const WOPNOperator &b = before.operators[op];
        const WOPNOperator &a = after.operators[op];
        field("dtfm_30", b.dtfm_30, a.dtfm_30);
        field("level_40", b.level_40, a.level_40);
        field("rsatk_50", b.rsatk_50, a.rsatk_50);
        field("amdecay1_60", b.amdecay1_60, a.amdecay1_60);
        field("decay2_70", b.decay2_70, a.decay2_70);
        field("susrel_80", b.susrel_80, a.susrel_80);
        field("ssgeg_90", b.ssgeg_90, a.ssgeg_90);
    }
#endif
    field("delay_on_ms", before.delay_on_ms, after.delay_on_ms);
    field("delay_off_ms", before.delay_off_ms, after.delay_off_ms);
}

// Converting each instrument of a bank to the file format, or to the
// properties of a saved state, and back gives the same instrument.
bool instruments_round_trip(const std::string &name, const WOPx::Bank &bank)
{
    bool same = true;
    for (unsigned program = 0; program < 128; ++program) {
        const Instrument ins = Instrument::from_wopl(bank.ins[program]);
        const Instrument through_file = Instrument::from_wopl(ins.to_wopl());
        const Instrument through_state = Instrument::from_properties(ins.to_properties());
        if (!ins.equal_instrument(through_file)) {
            print_differences(name, "file format", program, ins.to_wopl(), through_file.to_wopl());
            same = false;
        }
        if (!ins.equal_instrument(through_state)) {
            print_differences(name, "saved state", program, ins.to_wopl(), through_state.to_wopl());
            same = false;
        }
    }
    return same;
}

}  // namespace

ADLPLUG_TEST(embedded_banks)
{
    Pak_File_Reader pak;
    CHECK(pak.init_with_data(Res::banks_pak.data, Res::banks_pak.size));
    CHECK(pak.entry_count() > 0);

    for (std::size_t i = 0; i < pak.entry_count(); ++i) {
        const std::string &name = pak.name(i);
        std::string data = pak.extract(i);
        int error = 0;
        const WOPx::BankFile_Ptr file(WOPx::LoadBankFromMem(data.data(), data.size(), &error));
        if (file == nullptr) {
            std::fprintf(stderr, "%s: cannot be loaded (error %d)\n", name.c_str(), error);
            CHECK(file != nullptr);
            continue;
        }

        // Saved again in its own version and loaded back, the bank is the same.
        std::vector<char> saved(WOPx::CalculateBankFileSize(file.get(), file->version));
        CHECK(WOPx::SaveBankToMem(file.get(), saved.data(), saved.size(), file->version, 0) == 0);
        const WOPx::BankFile_Ptr reloaded(WOPx::LoadBankFromMem(saved.data(), saved.size(), &error));
        CHECK(reloaded != nullptr && WOPx::BanksCmp(file.get(), reloaded.get()) == 1);

        bool same = true;
        for (unsigned b = 0; b < file->banks_count_melodic; ++b)
            same = instruments_round_trip(name + " (melodic)", file->banks_melodic[b]) && same;
        for (unsigned b = 0; b < file->banks_count_percussion; ++b)
            same = instruments_round_trip(name + " (percussion)", file->banks_percussive[b]) && same;
        CHECK(same);

        // The bank's global parameters go through a state unchanged.
        std::vector<Midi_Bank> banks;
        Instrument_Global_Parameters igp;
        Midi_Bank::from_wopl(*file, banks, igp);
        CHECK(Instrument_Global_Parameters::from_properties(igp.to_properties()) == igp);
#if defined(ADLPLUG_OPL3)
        CHECK(igp.mt32_defaults == ((file->opl_flags & WOPL_FLAG_MT32) != 0));
#endif
    }
}

ADLPLUG_TEST(instrument_flags_in_state)
{
    // The flags that no parameter holds come back from a saved state as well.
#if defined(ADLPLUG_OPL3)
    for (int mode = 0; mode <= 5; ++mode) {
        for (const bool fixed : {false, true}) {
            Instrument ins;
            ins.blank(false);
            ins.rhythm_mode(mode);
            ins.fixed_note(fixed);
            const Instrument restored = Instrument::from_properties(ins.to_properties());
            CHECK(restored.rhythm_mode() == mode && restored.fixed_note() == fixed);
            CHECK(restored.equal_instrument(ins));
        }
    }

    // States saved before the flags were kept read as neither.
    juce::PropertySet old_state = Instrument().to_properties();
    old_state.removeValue("rhythm_mode");
    old_state.removeValue("fixed_note");
    const Instrument from_old = Instrument::from_properties(old_state);
    CHECK(from_old.rhythm_mode() == 0 && !from_old.fixed_note());

    // A damaged state cannot select a drum type that does not exist.
    juce::PropertySet damaged = Instrument().to_properties();
    damaged.setValue("rhythm_mode", 7);
    CHECK(Instrument::from_properties(damaged).rhythm_mode() == 5);
#elif defined(ADLPLUG_OPN2)
    for (const bool pseudo : {false, true}) {
        Instrument ins;
        ins.blank(false);
        ins.pseudo_eight_op(pseudo);
        const Instrument restored = Instrument::from_properties(ins.to_properties());
        CHECK(restored.pseudo_eight_op() == pseudo);
        CHECK(restored.equal_instrument(ins));
    }
#endif
}

ADLPLUG_TEST(bank_id_integer)
{
    bool same = true;
    for (unsigned msb = 0; msb < 128; ++msb) {
        for (unsigned lsb = 0; lsb < 128; ++lsb) {
            for (const bool percussive : {false, true}) {
                const Bank_Id id(static_cast<std::uint8_t>(msb), static_cast<std::uint8_t>(lsb), percussive);
                same = same && Bank_Id::from_integer(id.to_integer()) == id;
            }
        }
    }
    CHECK(same);
    // Bits outside the three fields are ignored.
    CHECK(Bank_Id::from_integer(0xffffffffu) == Bank_Id(127, 127, true));
}
