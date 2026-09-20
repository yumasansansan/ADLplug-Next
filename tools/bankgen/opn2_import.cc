// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

#include "opn2_import.h"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <format>
#include <optional>

namespace {

// A bank with the given numbers of melodic and percussion banks, all of their
// instruments blank, as OPN2 Bank Editor starts a bank it reads.
WOPNFile_Ptr blank_file(std::size_t melodic, std::size_t percussion)
{
    WOPNFile_Ptr file(WOPN_Init(static_cast<std::uint16_t>(melodic), static_cast<std::uint16_t>(percussion)));
    if (!file)
        return file;
    file->version = 2;
    file->lfo_freq = 0;
    file->chip_type = WOPN_Chip_OPN2;
    file->volume_model = WOPN_VM_Generic;
    const auto clear = [](WOPNBank &bank) {
        bank = WOPNBank {};
        for (WOPNInstrument &ins : bank.ins)
            ins.inst_flags = WOPN_Ins_IsBlank;
    };
    for (std::size_t i = 0; i < melodic; ++i)
        clear(file->banks_melodic[i]);
    for (std::size_t i = 0; i < percussion; ++i)
        clear(file->banks_percussive[i]);
    return file;
}

// The register values of an operator, with the bits that OPN2 Bank Editor
// keeps of each (FmBank::Instrument::setRegDUMUL() and the others).
void set_operator(WOPNOperator &op, const std::uint8_t reg[7])
{
    op.dtfm_30 = reg[0] & 0x7f;
    op.level_40 = reg[1] & 0x7f;
    op.rsatk_50 = reg[2] & 0xdf;
    op.amdecay1_60 = reg[3] & 0x9f;
    op.decay2_70 = reg[4] & 0x1f;
    op.susrel_80 = reg[5];
    op.ssgeg_90 = reg[6] & 0x0f;
}

void add_note(std::string &notes, const std::string &sentence)
{
    notes += sentence;
    notes += '\n';
}

}  // namespace

bool import_gyb(std::span<const std::uint8_t> data, Imported_Bank &bank, std::string &error)
{
    const auto fail = [&error](std::string what) {
        error = std::move(what);
        return false;
    };

    std::size_t pos = 0;
    const auto take = [&data, &pos](std::size_t count) -> const std::uint8_t * {
        if (data.size() - pos < count)
            return nullptr;
        const std::uint8_t *p = data.data() + pos;
        pos += count;
        return p;
    };

    const std::uint8_t *header = take(5);
    if (!header || header[0] != 26 || header[1] != 12)
        return fail("not a GYB bank");
    const unsigned version = header[2];
    if (version != 1 && version != 2)
        return fail(std::format("GYB version {} is not supported", version));
    const unsigned melodic_count = header[3];
    const unsigned drum_count = header[4];
    if (melodic_count > 128 || drum_count > 128)
        return fail("more than 128 instruments of a kind");

    // For each GM program and drum, the index of the instrument that plays it.
    const std::uint8_t *map = take(256);
    if (!map)
        return fail("truncated");

    WOPNFile_Ptr file = blank_file(1, 1);
    if (!file)
        return fail("out of memory");
    if (version == 2) {
        const std::uint8_t *lfo = take(1);
        if (!lfo)
            return fail("truncated");
        file->lfo_freq = *lfo & 0x0f;
    }

    // Where an instrument goes: the first slot that the map gives it, or else
    // the slot of its own index if the map leaves that free; or nowhere.
    const auto slot_of = [map, melodic_count](unsigned index) -> std::optional<unsigned> {
        const unsigned drum = (index >= melodic_count) ? 1 : 0;
        const unsigned own = index - drum * melodic_count;
        for (unsigned slot = 0; slot < 128; ++slot) {
            if (map[2 * slot + drum] == own)
                return slot;
        }
        if (map[2 * own + drum] >= 128)
            return own;
        return std::nullopt;
    };
    const auto instrument_at = [&file, melodic_count](unsigned index, unsigned slot) -> WOPNInstrument & {
        const bool drum = index >= melodic_count;
        return (drum ? file->banks_percussive : file->banks_melodic)[0].ins[slot];
    };

    const unsigned total = melodic_count + drum_count;
    const std::size_t record_size = (version == 2) ? 32 : 30;
    unsigned unplaced = 0;
    unsigned panned = 0;
    for (unsigned index = 0; index < total; ++index) {
        const std::uint8_t *r = take(record_size);
        if (!r)
            return fail("truncated");
        const std::optional<unsigned> slot = slot_of(index);
        if (!slot) {
            ++unplaced;
            continue;
        }

        WOPNInstrument &ins = instrument_at(index, *slot);
        ins = WOPNInstrument {};
        // The registers of the four operators, each kind in slot order.
        for (unsigned op = 0; op < 4; ++op) {
            const std::uint8_t reg[7] {r[op], r[4 + op], r[8 + op], r[12 + op], r[16 + op], r[20 + op], r[24 + op]};
            set_operator(ins.operators[op], reg);
        }
        ins.fbalg = r[28] & 0x3f;
        if (version == 2) {
            ins.lfosens = r[29] & 0x37;
            if ((r[29] & 0xc0) != 0)
                ++panned;
        }
        const std::uint8_t transpose_or_key = r[(version == 2) ? 30 : 29];
        if (index < melodic_count)
            ins.note_offset = static_cast<std::int16_t>(-static_cast<std::int8_t>(transpose_or_key));
        else
            ins.percussion_key_number = transpose_or_key & 127;
    }

    for (unsigned index = 0; index < total; ++index) {
        const std::uint8_t *length = take(1);
        const std::uint8_t *text = length ? take(*length) : nullptr;
        if (!text)
            return fail("truncated");
        if (const std::optional<unsigned> slot = slot_of(index)) {
            WOPNInstrument &ins = instrument_at(index, *slot);
            std::memcpy(ins.inst_name, text, std::min<std::size_t>(*length, sizeof ins.inst_name));
        }
    }

    bank.notes.clear();
    if (unplaced != 0)
        add_note(bank.notes, std::format(
            "{} instruments have no program or drum of their own, as OPN2 Bank Editor places them, and are left out.",
            unplaced));
    if (panned != 0)
        add_note(bank.notes, std::format(
            "{} instruments set the outputs of the channel; the WOPN bank does not keep them.", panned));
    bank.file = std::move(file);
    return true;
}

bool import_gems(std::span<const std::uint8_t> data, Imported_Bank &bank, std::string &error)
{
    const auto fail = [&error](std::string what) {
        error = std::move(what);
        return false;
    };

    // A name of 100 bytes, then the patches. The size counts 84 bytes for each,
    // but OPN2 Bank Editor reads them 80 bytes apart, and in the example banks
    // that is where they are: 4 bytes for each follow all of them.
    constexpr std::size_t name_size = 100;
    constexpr std::size_t patch_size = 80;
    if (data.size() < name_size || (data.size() - name_size) % (patch_size + 4) != 0)
        return fail("not a GEMS bank");
    const std::size_t count = (data.size() - name_size) / (patch_size + 4);
    const std::size_t bank_count = (count + 127) / 128;
    if (bank_count > std::size_t{128} * 128)
        return fail("too many patches");

    WOPNFile_Ptr file = blank_file(bank_count, 1);
    if (!file)
        return fail("out of memory");
    const std::uint8_t *name = data.data();
    const std::size_t name_length = static_cast<std::size_t>(
        std::find(name, name + name_size, 0) - name);
    for (std::size_t i = 0; i < bank_count; ++i) {
        WOPNBank &b = file->banks_melodic[i];
        std::memcpy(b.bank_name, name, std::min(name_length, sizeof b.bank_name - 1));
        b.bank_midi_msb = static_cast<std::uint8_t>(i / 128);
        b.bank_midi_lsb = static_cast<std::uint8_t>(i % 128);
    }

    unsigned not_fm = 0;
    unsigned with_lfo = 0;
    unsigned with_ch3 = 0;
    unsigned panned = 0;
    unsigned with_disabled = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint8_t *p = data.data() + name_size + i * patch_size;
        // Patches of other kinds (PSG, noise, samples) have their own codes.
        if ((p[0] << 8 | p[1]) != 0) {
            ++not_fm;
            continue;
        }

        WOPNInstrument &ins = file->banks_melodic[i / 128].ins[i % 128];
        ins = WOPNInstrument {};
        const std::uint8_t *patch_name = p + 2;
        std::memcpy(ins.inst_name, patch_name,
                    static_cast<std::size_t>(std::find(patch_name, patch_name + 28, 0) - patch_name));

        if ((p[30] & 0x08) != 0)
            ++with_lfo;
        if ((p[31] & 0x40) != 0)
            ++with_ch3;
        if ((p[33] & 0xc0) != 0xc0)
            ++panned;

        ins.fbalg = p[32] & 0x3f;
        ins.lfosens = p[33] & 0x37;
        for (unsigned op = 0; op < 4; ++op) {
            const std::uint8_t *o = p + 34 + std::size_t{6} * op;
            const std::uint8_t reg[7] {o[0], o[1], o[2], o[3], o[4], o[5], 0};
            set_operator(ins.operators[op], reg);
        }

        // An operator that the patch turns off gets the lowest level. The bits
        // go in the order of the operators' numbers, 1 to 4.
        static constexpr unsigned slot_of_number[4] {0, 2, 1, 3};
        bool disabled = false;
        for (unsigned bit = 0; bit < 4; ++bit) {
            if ((p[66] & (1u << bit)) == 0) {
                ins.operators[slot_of_number[bit]].level_40 = 127;
                disabled = true;
            }
        }
        if (disabled)
            ++with_disabled;
    }

    bank.notes.clear();
    if (not_fm != 0)
        add_note(bank.notes, std::format("{} of the {} patches are not FM patches, and are left blank.", not_fm, count));
    if (with_lfo != 0)
        add_note(bank.notes, std::format(
            "{} patches turn on the LFO, each at a frequency of its own; a WOPN bank has one LFO setting for all its instruments, which is left off.",
            with_lfo));
    if (with_ch3 != 0)
        add_note(bank.notes, std::format(
            "{} patches play in the special mode of channel 3, with a frequency for each operator; the WOPN bank does not keep it.",
            with_ch3));
    if (panned != 0)
        add_note(bank.notes, std::format(
            "{} patches play on one side only; the WOPN bank does not keep that, and the MIDI pan places them.", panned));
    if (with_disabled != 0)
        add_note(bank.notes, std::format(
            "{} patches turn operators off, which get the lowest level instead.", with_disabled));
    bank.file = std::move(file);
    return true;
}
