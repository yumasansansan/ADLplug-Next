// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

#include "woplx.h"
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

std::string_view trim(std::string_view text)
{
    const auto blank = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!text.empty() && blank(text.front()))
        text.remove_prefix(1);
    while (!text.empty() && blank(text.back()))
        text.remove_suffix(1);
    return text;
}

bool to_integer(std::string_view text, long min, long max, long &value)
{
    text = trim(text);
    const char *end = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(text.data(), end, value);
    return !text.empty() && ec == std::errc() && ptr == end && value >= min && value <= max;
}

struct Field {
    std::string_view key;
    std::string_view value;
};

// Splits "KEY=value;KEY=value;" into its fields.
bool split_fields(std::string_view text, std::vector<Field> &fields)
{
    fields.clear();
    while (!text.empty()) {
        const std::size_t semicolon = text.find(';');
        const std::string_view item = trim(text.substr(0, semicolon));
        text = (semicolon == std::string_view::npos) ? std::string_view() : text.substr(semicolon + 1);
        if (item.empty())
            continue;
        const std::size_t equals = item.find('=');
        if (equals == std::string_view::npos)
            return false;
        fields.push_back({trim(item.substr(0, equals)), trim(item.substr(equals + 1))});
    }
    return true;
}

// Copies a name into a fixed field, which keeps a terminating zero.
bool copy_name(std::string_view name, char *field, std::size_t field_size)
{
    if (name.size() >= field_size)
        return false;
    std::memset(field, 0, field_size);
    std::memcpy(field, name.data(), name.size());
    return true;
}

// Where each operator key goes: its register, the shift and the largest value.
struct Operator_Key {
    std::string_view key;
    std::uint8_t WOPLOperator::*reg;
    unsigned shift;
    long max;
};

constexpr std::array<Operator_Key, 12> operator_keys {{
    {"AT", &WOPLOperator::atdec_60, 4, 15},
    {"DC", &WOPLOperator::atdec_60, 0, 15},
    {"ST", &WOPLOperator::susrel_80, 4, 15},
    {"RL", &WOPLOperator::susrel_80, 0, 15},
    {"WF", &WOPLOperator::waveform_E0, 0, 7},
    {"ML", &WOPLOperator::avekf_20, 0, 15},
    {"TL", &WOPLOperator::ksl_l_40, 0, 63},
    {"KL", &WOPLOperator::ksl_l_40, 6, 3},
    {"VB", &WOPLOperator::avekf_20, 6, 1},
    {"AM", &WOPLOperator::avekf_20, 7, 1},
    {"EG", &WOPLOperator::avekf_20, 5, 1},
    {"KR", &WOPLOperator::avekf_20, 4, 1},
}};

// The rhythm-mode drum types of the RHYTHM attribute, from 6 on.
constexpr std::array<std::uint8_t, 5> rhythm_modes {
    WOPL_RM_BassDrum, WOPL_RM_Snare, WOPL_RM_TomTom, WOPL_RM_Cymbal, WOPL_RM_HiHat,
};

class Reader {
public:
    bool read(const std::string &text, Woplx_Bank &out, std::string &error);

private:
    enum class Level { top, bank, instrument };

    std::string *error_ = nullptr;
    std::size_t line_number_ = 0;
    std::vector<Field> fields_;

    std::uint8_t flags_ = 0;
    std::uint8_t volume_model_ = 0;
    std::vector<WOPLBank> melodic_;
    std::vector<WOPLBank> percussion_;

    Level level_ = Level::top;
    bool percussive_ = false;
    WOPLBank bank_ {};
    bool have_msb_ = false;
    bool have_lsb_ = false;
    std::array<bool, 128> seen_ {};
    WOPLInstrument *instrument_ = nullptr;

    bool fail(std::string_view what);
    bool read_top(std::string_view line);
    bool read_bank(std::string_view line);
    bool read_instrument(std::string_view line);
    bool start_instrument(std::string_view number);
    bool end_bank(std::string_view line);
    bool read_flags(std::string_view text);
    bool read_attributes(std::string_view text);
    bool read_connections(std::string_view text);
    bool read_operator(unsigned op, std::string_view text);
};

bool Reader::fail(std::string_view what)
{
    *error_ = "line " + std::to_string(line_number_) + ": " + std::string(what);
    return false;
}

bool Reader::read(const std::string &text, Woplx_Bank &out, std::string &error)
{
    error_ = &error;
    bool first = true;
    bool in_info = false;

    for (std::size_t pos = 0; pos < text.size();) {
        std::size_t end = text.find('\n', pos);
        if (end == std::string::npos)
            end = text.size();
        std::string_view raw(text.data() + pos, end - pos);
        pos = end + 1;
        ++line_number_;
        if (!raw.empty() && raw.back() == '\r')
            raw.remove_suffix(1);
        const std::string_view line = trim(raw);

        if (first) {
            if (line != "WOPLX-BANK")
                return fail("not a WOPLX bank");
            first = false;
            continue;
        }
        if (in_info) {
            if (line == "BANK_INFO_END")
                in_info = false;
            else {
                out.info.append(raw);
                out.info.push_back('\n');
            }
            continue;
        }
        if (line.empty() || line.starts_with("#") || line.starts_with("//"))
            continue;

        bool ok = false;
        switch (level_) {
        case Level::top:
            if (line == "BANK_INFO:") {
                in_info = true;
                ok = true;
            }
            else
                ok = read_top(line);
            break;
        case Level::bank:
            ok = read_bank(line);
            break;
        case Level::instrument:
            ok = read_instrument(line);
            break;
        }
        if (!ok)
            return false;
    }

    if (first)
        return fail("empty file");
    if (in_info)
        return fail("BANK_INFO without BANK_INFO_END");
    if (level_ != Level::top)
        return fail("bank without its end");

    // Allocated as WOPL_Init() does, but with the counts the file has, zero
    // included, which WOPL_Init() would raise to one.
    WOPLFile_Ptr file(static_cast<WOPLFile *>(std::calloc(1, sizeof(WOPLFile))));
    if (!file)
        return fail("out of memory");
    file->version = 3;
    file->opl_flags = flags_;
    file->volume_model = volume_model_;
    file->banks_count_melodic = static_cast<std::uint16_t>(melodic_.size());
    file->banks_count_percussion = static_cast<std::uint16_t>(percussion_.size());
    file->banks_melodic = static_cast<WOPLBank *>(std::calloc(melodic_.size() + 1, sizeof(WOPLBank)));
    file->banks_percussive = static_cast<WOPLBank *>(std::calloc(percussion_.size() + 1, sizeof(WOPLBank)));
    if (!file->banks_melodic || !file->banks_percussive)
        return fail("out of memory");
    for (std::size_t i = 0; i < melodic_.size(); ++i)
        file->banks_melodic[i] = melodic_[i];
    for (std::size_t i = 0; i < percussion_.size(); ++i)
        file->banks_percussive[i] = percussion_[i];
    out.file = std::move(file);
    return true;
}

bool Reader::read_top(std::string_view line)
{
    const bool melodic = line == "MELODIC_BANK:";
    if (melodic || line == "PERCUSSION_BANK:") {
        level_ = Level::bank;
        percussive_ = !melodic;
        bank_ = WOPLBank {};
        for (WOPLInstrument &ins : bank_.ins)
            ins.inst_flags = WOPL_Ins_IsBlank;
        have_msb_ = have_lsb_ = false;
        seen_.fill(false);
        instrument_ = nullptr;
        return true;
    }

    const std::size_t equals = line.find('=');
    if (equals == std::string_view::npos)
        return fail("unknown line");
    const std::string_view key = line.substr(0, equals);
    long value = 0;
    const auto bit = [&](std::uint8_t flag) {
        if (!to_integer(line.substr(equals + 1), 0, 1, value))
            return fail("a flag must be 0 or 1");
        flags_ = static_cast<std::uint8_t>(value ? (flags_ | flag) : (flags_ & ~flag));
        return true;
    };
    if (key == "DEEP_TREMOLO")
        return bit(WOPL_FLAG_DEEP_TREMOLO);
    if (key == "DEEP_VIBRATO")
        return bit(WOPL_FLAG_DEEP_VIBRATO);
    if (key == "IS_MT32")
        return bit(WOPL_FLAG_MT32);
    if (key == "VOLUME_MODEL") {
        if (!to_integer(line.substr(equals + 1), 0, 255, value))
            return fail("bad volume model");
        volume_model_ = static_cast<std::uint8_t>(value);
        return true;
    }
    return fail("unknown key " + std::string(key));
}

bool Reader::read_bank(std::string_view line)
{
    long value = 0;
    if (line.starts_with("NAME=")) {
        if (!copy_name(line.substr(5), bank_.bank_name, sizeof bank_.bank_name))
            return fail("bank name too long");
        return true;
    }
    if (line.starts_with("MIDI_BANK_MSB=")) {
        if (!to_integer(line.substr(14), 0, 127, value))
            return fail("bad MIDI bank MSB");
        bank_.bank_midi_msb = static_cast<std::uint8_t>(value);
        have_msb_ = true;
        return true;
    }
    if (line.starts_with("MIDI_BANK_LSB=")) {
        if (!to_integer(line.substr(14), 0, 127, value))
            return fail("bad MIDI bank LSB");
        bank_.bank_midi_lsb = static_cast<std::uint8_t>(value);
        have_lsb_ = true;
        return true;
    }
    if (line.starts_with("INSTRUMENT=")) {
        if (!have_msb_ || !have_lsb_)
            return fail("instrument before the MIDI bank numbers");
        level_ = Level::instrument;
        return start_instrument(line.substr(11));
    }
    return end_bank(line);
}

bool Reader::read_instrument(std::string_view line)
{
    if (line.starts_with("INSTRUMENT="))
        return start_instrument(line.substr(11));
    if (line.starts_with("NAME=")) {
        if (!copy_name(line.substr(5), instrument_->inst_name, 33))
            return fail("instrument name too long");
        return true;
    }
    if (line.starts_with("FLAGS:"))
        return read_flags(line.substr(6));
    if (line.starts_with("ATTRS:"))
        return read_attributes(line.substr(6));
    if (line.starts_with("FBCONN:"))
        return read_connections(line.substr(7));
    if (line.size() >= 4 && line.starts_with("OP") && line[3] == ':' && line[2] >= '0' && line[2] <= '3')
        return read_operator(static_cast<unsigned>(line[2] - '0'), line.substr(4));
    return end_bank(line);
}

bool Reader::start_instrument(std::string_view number)
{
    number = trim(number);
    if (!number.ends_with(":"))
        return fail("malformed instrument number");
    long program = 0;
    if (!to_integer(number.substr(0, number.size() - 1), 0, 127, program))
        return fail("bad instrument number");
    if (seen_[static_cast<std::size_t>(program)])
        return fail("instrument given twice");
    seen_[static_cast<std::size_t>(program)] = true;
    instrument_ = &bank_.ins[program];
    *instrument_ = WOPLInstrument {};
    return true;
}

bool Reader::end_bank(std::string_view line)
{
    if (line != (percussive_ ? "PERCUSSION_BANK_END" : "MELODIC_BANK_END"))
        return fail("unknown line");
    if (!have_msb_ || !have_lsb_)
        return fail("bank without MIDI bank numbers");
    (percussive_ ? percussion_ : melodic_).push_back(bank_);
    level_ = Level::top;
    instrument_ = nullptr;
    return true;
}

bool Reader::read_flags(std::string_view text)
{
    unsigned voices = 0;
    while (!text.empty()) {
        const std::size_t semicolon = text.find(';');
        const std::string_view token = trim(text.substr(0, semicolon));
        text = (semicolon == std::string_view::npos) ? std::string_view() : text.substr(semicolon + 1);
        if (token.empty())
            continue;
        if (token == "FN")
            instrument_->inst_flags |= WOPL_Ins_FixedNote;
        else if (token == "2OP")
            ++voices;
        else if (token == "4OP") {
            instrument_->inst_flags |= WOPL_Ins_4op;
            ++voices;
        }
        else if (token == "DV") {
            instrument_->inst_flags |= WOPL_Ins_4op | WOPL_Ins_Pseudo4op;
            ++voices;
        }
        else
            return fail("unknown flag " + std::string(token));
    }
    if (voices != 1)
        return fail("an instrument needs one of 2OP, 4OP and DV");
    return true;
}

bool Reader::read_attributes(std::string_view text)
{
    if (!split_fields(text, fields_))
        return fail("malformed attributes");
    WOPLInstrument &ins = *instrument_;
    for (const Field &f : fields_) {
        long v = 0;
        const auto value = [&](long min, long max) { return to_integer(f.value, min, max, v); };
        if (f.key == "DRUM_KEY" && value(0, 255))
            ins.percussion_key_number = static_cast<std::uint8_t>(v);
        else if (f.key == "NOTE_OFF_1" && value(-32768, 32767))
            ins.note_offset1 = static_cast<std::int16_t>(v);
        else if (f.key == "NOTE_OFF_2" && value(-32768, 32767))
            ins.note_offset2 = static_cast<std::int16_t>(v);
        else if (f.key == "VEL_OFF" && value(-128, 127))
            ins.midi_velocity_offset = static_cast<std::int8_t>(v);
        else if (f.key == "FINE_TUNE" && value(-128, 127))
            ins.second_voice_detune = static_cast<std::int8_t>(v);
        else if (f.key == "RHYTHM" && value(0, 10) && (v == 0 || v >= 6)) {
            if (v != 0)
                ins.inst_flags |= rhythm_modes[static_cast<std::size_t>(v - 6)];
        }
        else if (f.key == "DUR_K_ON" && value(0, 65535))
            ins.delay_on_ms = static_cast<std::uint16_t>(v);
        else if (f.key == "DUR_K_OFF" && value(0, 65535))
            ins.delay_off_ms = static_cast<std::uint16_t>(v);
        else
            return fail("unknown or bad attribute " + std::string(f.key));
    }
    return true;
}

bool Reader::read_connections(std::string_view text)
{
    if (!split_fields(text, fields_))
        return fail("malformed feedback and connections");
    WOPLInstrument &ins = *instrument_;
    for (const Field &f : fields_) {
        long v = 0;
        if (f.key == "FB1" && to_integer(f.value, 0, 7, v))
            ins.fb_conn1_C0 = static_cast<std::uint8_t>(ins.fb_conn1_C0 | v << 1);
        else if (f.key == "CONN1" && to_integer(f.value, 0, 1, v))
            ins.fb_conn1_C0 = static_cast<std::uint8_t>(ins.fb_conn1_C0 | v);
        else if (f.key == "FB2" && to_integer(f.value, 0, 7, v))
            ins.fb_conn2_C0 = static_cast<std::uint8_t>(ins.fb_conn2_C0 | v << 1);
        else if (f.key == "CONN2" && to_integer(f.value, 0, 1, v))
            ins.fb_conn2_C0 = static_cast<std::uint8_t>(ins.fb_conn2_C0 | v);
        else
            return fail("unknown or bad connection field " + std::string(f.key));
    }
    return true;
}

// OP0 to OP3 are carrier 1, modulator 1, carrier 2 and modulator 2, the order
// of WOPL's operators.
bool Reader::read_operator(unsigned op, std::string_view text)
{
    if (!split_fields(text, fields_))
        return fail("malformed operator");
    WOPLOperator &o = instrument_->operators[op];
    for (const Field &f : fields_) {
        bool known = false;
        for (const Operator_Key &k : operator_keys) {
            long v = 0;
            if (f.key != k.key)
                continue;
            if (!to_integer(f.value, 0, k.max, v))
                return fail("bad value of operator field " + std::string(f.key));
            o.*k.reg = static_cast<std::uint8_t>(o.*k.reg | v << k.shift);
            known = true;
            break;
        }
        if (!known)
            return fail("unknown operator field " + std::string(f.key));
    }
    return true;
}

}  // namespace

bool read_woplx(const std::string &text, Woplx_Bank &bank, std::string &error)
{
    return Reader().read(text, bank, error);
}
