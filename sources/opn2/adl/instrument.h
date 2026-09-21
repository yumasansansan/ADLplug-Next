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

#pragma once
#include "utility/field_bitops.h"
#include <wopn/wopn_file.h>
#include <opnmidi.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>
namespace juce { class PropertySet; }

struct Instrument : OPN2_Instrument
{
    Instrument() noexcept
        : OPN2_Instrument{} { inst_flags = OPNMIDI_Ins_IsBlank; }

    static constexpr int latest_version = OPNMIDI_InstrumentVersion;

    static Instrument from_adlmidi(const OPN2_Instrument &o) noexcept;
    static Instrument from_wopl(const WOPNInstrument &o) noexcept;
    WOPNInstrument to_wopl() const noexcept;

    juce::PropertySet to_properties() const;
    static Instrument from_properties(const juce::PropertySet &set);

    // Accessors for the fields packed into the register bytes.
#define PARAMETER(type, id, field, shift, size, opt)                    \
    type id() const noexcept                                            \
        { return Field_Bitops::get##opt<shift, size, type>(field); }    \
    void id(type value) noexcept                                        \
        { Field_Bitops::set##opt<shift, size>(field, value); }

#define OP_PARAMETER(type, id, field, shift, size, opt)                 \
    type id(unsigned op) const noexcept                                 \
        { return Field_Bitops::get##opt<shift, size, type>(operators[op].field); } \
    void id(unsigned op, type value) noexcept                           \
        { Field_Bitops::set##opt<shift, size>(operators[op].field, value); }

    PARAMETER(bool, blank, inst_flags, 1, 1,)
    PARAMETER(bool, pseudo_eight_op, inst_flags, 0, 1,)
    PARAMETER(int, feedback, fbalg, 3, 3,)
    PARAMETER(int, algorithm, fbalg, 0, 3,)
    PARAMETER(int, ams, lfosens, 4, 2,)
    PARAMETER(int, fms, lfosens, 0, 3,)
    OP_PARAMETER(int, detune, dtfm_30, 4, 3,)
    OP_PARAMETER(int, fmul, dtfm_30, 0, 4,)
    OP_PARAMETER(int, level, level_40, 0, 7, _inverted)
    OP_PARAMETER(int, ratescale, rsatk_50, 6, 2,)
    OP_PARAMETER(int, attack, rsatk_50, 0, 5,)
    OP_PARAMETER(bool, am, amdecay1_60, 7, 1,)
    OP_PARAMETER(int, decay1, amdecay1_60, 0, 5,)
    OP_PARAMETER(int, decay2, decay2_70, 0, 5,)
    OP_PARAMETER(int, sustain, susrel_80, 4, 4,)
    OP_PARAMETER(int, release, susrel_80, 0, 4,)
    OP_PARAMETER(bool, ssgenable, ssgeg_90, 3, 1,)
    OP_PARAMETER(int, ssgwave, ssgeg_90, 0, 3,)

#undef PARAMETER
#undef OP_PARAMETER

    char name[32] = {};

    void describe(std::FILE *out) const noexcept;
    void describe_operator(unsigned op, std::FILE *out, const char *indent = "") const noexcept;

    bool equal_instrument(const OPN2_Instrument &o) const noexcept;
    bool equal_instrument_except_delays(const OPN2_Instrument &o) const noexcept;
};

struct Instrument_Global_Parameters
{
    int volume_model = 0;
    bool lfo_enable = false;
    int lfo_frequency = 0;

    bool operator==(const Instrument_Global_Parameters &) const = default;

    juce::PropertySet to_properties() const;
    static Instrument_Global_Parameters from_properties(const juce::PropertySet &set);
};

struct Bank_Ref : OPN2_Bank
{
    constexpr Bank_Ref() noexcept
        : OPN2_Bank{} {}
};

struct Bank_Id : OPN2_BankId
{
    constexpr Bank_Id() noexcept
        : OPN2_BankId{0, 0xff, 0xff} {}
    constexpr Bank_Id(std::uint8_t bank_msb, std::uint8_t bank_lsb, bool is_percussive) noexcept
        : OPN2_BankId{static_cast<OPN2_UInt8>(is_percussive), bank_msb, bank_lsb} {}
    // A bank the library can hold: it numbers one with two seven-bit halves and
    // refuses anything else, so a number out of that range is no bank -- the id
    // of an empty slot, which fills both halves, among them. The numbers come
    // from outside: a bank file names its own, and so does the state of a
    // project.
    constexpr explicit operator bool() const noexcept
        { return msb <= 0x7f && lsb <= 0x7f; }
    constexpr bool operator==(const Bank_Id &o) const noexcept
        { return msb == o.msb && lsb == o.lsb && (percussive != 0) == (o.percussive != 0); }
    constexpr std::uint32_t pseudo_id() const noexcept
        { return (msb & 127u) << 7 | (lsb & 127u); }
    constexpr std::uint32_t to_integer() const noexcept
        { return (msb & 127u) << 8 | (lsb & 127u) << 1 | (percussive & 1u); }
    static constexpr Bank_Id from_integer(std::uint32_t x) noexcept
        { return Bank_Id(static_cast<std::uint8_t>((x >> 8) & 127), static_cast<std::uint8_t>((x >> 1) & 127), (x & 1) != 0); }
};

inline bool operator<(Bank_Id a, Bank_Id b) noexcept
{
    return a.to_integer() < b.to_integer();
}

struct Midi_Bank
{
    Bank_Id id;
    std::array<Instrument, 128> ins;
    char name[32] = {};

    static void from_wopl(const WOPNFile &wopl, std::vector<Midi_Bank> &banks, Instrument_Global_Parameters &igp);
};
