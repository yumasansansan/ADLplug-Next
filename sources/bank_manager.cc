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

#include "bank_manager.h"
#include "plugin_processor.h"
#include "worker.h"
#include "messages.h"
#include "adl/player.h"
#include "adl/wopx_file.h"
#include "utility/name_field.h"
#include <algorithm>
#include <cassert>
#include <cstring>

#if 1
#   define trace(fmt, ...) ((void)0)
#else
#   pragma message("enabled debug messages which compromise hard realtime")
#   define trace(fmt, ...) std::fprintf(stderr, "[Bank Manager] " fmt "\n" __VA_OPT__(,) __VA_ARGS__)
#endif

namespace {

// Stores `name` in a name field as the text that fits, and says whether the
// field held it already.
//
// A name arrives as bytes -- a bank file writes what it likes in its name fields,
// and so a message may carry anything -- while the state of a project holds it as
// text. The text of bytes that are not UTF-8 is longer than the bytes it came
// from, so it need not fit back into a field of this size: what did not fit would
// be gone when a project was saved and read again, and the project would change
// by being opened. So the bytes are made text here, once, and the field keeps the
// text, of which a field's worth always fits.
bool assign_name(std::span<char, Bank_Manager::name_size> field, const char *name) noexcept
{
    std::array<char, Bank_Manager::name_size> stored {};
    copy_name_to_field(stored, name_from_field(std::span(name, utf8_fitting_length(name, stored.size()))));
    if (std::ranges::equal(field, stored))
        return false;
    std::ranges::copy(stored, field.begin());
    return true;
}

}  // namespace

Bank_Manager::Bank_Manager(AdlplugAudioProcessor &proc, Player &pl, std::span<std::uint8_t> wopl_data)
    : proc_(proc), pl_(pl)
{
    WOPx::BankFile_Ptr wopl;
    if (pl.load_bank_data(wopl_data.data(), wopl_data.size()))
        wopl.reset(WOPx::LoadBankFromMem(wopl_data.data(), wopl_data.size(), nullptr));

    initialize_all_banks();

    const unsigned nm = wopl ? unsigned{wopl->banks_count_melodic} : 0u;
    const unsigned np = wopl ? unsigned{wopl->banks_count_percussion} : 0u;

    for (unsigned b_i = 0; b_i < nm + np; ++b_i) {
        const bool percussive = b_i >= nm;
        const WOPx::Bank &bank = percussive ? wopl->banks_percussive[b_i - nm] : wopl->banks_melodic[b_i];
        const Bank_Id id(bank.bank_midi_msb, bank.bank_midi_lsb, percussive);
        rename_bank(id, bank.bank_name, false);
        for (unsigned p_i = 0; p_i < 128; ++p_i) {
            const WOPx::Instrument &ins = bank.ins[p_i];
            if ((ins.inst_flags & WOPx::Ins_IsBlank) == 0)
                rename_program(id, p_i, ins.inst_name, false);
        }
    }
}

void Bank_Manager::clear_banks(bool notify)
{
    trace("Clear banks");

    for (Bank_Info &info : bank_infos_) {
        if (!info)
            continue;
        pl_.ensure_remove_bank(info.bank);
        forget_bank(info);
    }

    if (notify)
        slots_notify_flag_ = true;
}

void Bank_Manager::mark_slots_for_notification()
{
    trace("Mark slots for notification");

    slots_notify_flag_ = true;
}

void Bank_Manager::mark_everything_for_notification()
{
    trace("Mark everything for notification");

    slots_notify_flag_ = true;
    for (Bank_Info &info : bank_infos_) {
        if (info)
            info.to_notify = info.used;
    }
}

void Bank_Manager::send_notifications()
{
    if (slots_notify_flag_) {
        if (!emit_slots())
            return;
        slots_notify_flag_ = false;
    }

    unsigned p_n = 0;
    for (Bank_Info &info : bank_infos_) {
        if (!info || info.to_notify.none())
            continue;
        for (unsigned p_i = 0; p_i < 128; ++p_i) {
            if (!info.to_notify.test(p_i))
                continue;
            if (!emit_notification(info, p_i))
                return;
            info.to_notify.reset(p_i);
            if (++p_n == max_program_notifications)
                return;
        }
    }
}

void Bank_Manager::send_measurement_requests()
{
    unsigned p_n = 0;
    for (Bank_Info &info : bank_infos_) {
        if (!info || info.to_measure.none())
            continue;
        for (unsigned p_i = 0; p_i < 128; ++p_i) {
            if (!info.to_measure.test(p_i))
                continue;
            assert(info.used.test(p_i));
            if (!emit_measurement_request(info, p_i))
                return;
            info.to_measure.reset(p_i);
            if (++p_n == max_program_measurements)
                return;
        }
    }
}

bool Bank_Manager::load_program(const Bank_Id &id, unsigned program, const Instrument &ins, unsigned flags)
{
    // What comes in is from outside: a bank file names its own bank numbers and
    // fills its own instruments (Midi_Bank::from_wopl), and so does the state of
    // a project. The player is the one that says whether it will hold them, so
    // nothing here is recorded that the player did not take: a number that is
    // not a bank it can hold is refused before a slot is given one, and the
    // calls below are all answered rather than assumed. Otherwise a slot would
    // keep a bank reference that was never set, or a program would count as
    // being in use while the player still had the instrument that was there.
    if (program >= program_count || !id)
        return false;

    Player &pl = pl_;

    std::optional<unsigned> slot = find_slot(id);
    if (slot) {
        trace("Loading program %c%u:%u:%u into existing bank at slot %u",
              id.percussive ? 'P' : 'M', id.msb, id.lsb, program, *slot);
    }
    else {
        // no slots, try to find empty
        slot = find_empty_slot();

        if (!slot) {
            trace("No empty slot to load program %c%u:%u:%u",
                  id.percussive ? 'P' : 'M', id.msb, id.lsb, program);
            return false;
        }

        Bank_Info &info = bank_infos_[*slot];
        if (!info.id) {
            trace("Loading program %c%u:%u:%u at empty slot %u",
                  id.percussive ? 'P' : 'M', id.msb, id.lsb, program, *slot);
        }
        else {
            // remove the old bank if one was there
            trace("Loading program %c%u:%u:%u over existing blank bank %c%u:%u at slot %u",
                  id.percussive ? 'P' : 'M', id.msb, id.lsb, program,
                  info.id.percussive ? 'P' : 'M', info.id.lsb, info.id.msb, *slot);
            pl.ensure_remove_bank(info.bank);
        }

        forget_bank(info);
        Bank_Ref bank;
        if (!pl.get_bank(id, Player::Bank_CreateRt, bank))
            return false;
        info.id = id;
        info.bank = bank;
    }

    Bank_Info &info = bank_infos_[*slot];

    Instrument old_ins;
    if (!pl.get_instrument(info.bank, program, old_ins))
        return false;

    const bool replace = (flags & LP_NoReplaceExisting) == 0 || old_ins.blank();
    if (!replace)
        return false;

    // The library takes an instrument of the version it knows and refuses any
    // other, so this can say no to what it was given.
    if (!pl.set_instrument(info.bank, program, ins))
        return false;

    // The name of the instrument, which the library does not keep, as the text
    // that fits (assign_name says why).
    static_assert(sizeof ins.name == name_size);
    if ((flags & LP_KeepName) == 0)
        assign_name(info.program_name(program), ins.name);

    // update program counts
    const std::size_t old_count = info.used.count();
    info.used.set(program, !ins.blank());
    if ((flags & LP_Notify) != 0 && info.used.count() != old_count)
        slots_notify_flag_ = true;

    // mark as needing measurement if necessary
    info.to_measure.set(program, (flags & LP_NeedMeasurement) != 0 && !ins.blank());

    // mark for notification
    if ((flags & LP_Notify) != 0)
        info.to_notify.set(program);
    return true;
}

bool Bank_Manager::delete_program(const Bank_Id &id, unsigned program, unsigned flags)
{
    trace("Deleting program %c%u:%u:%u",
          id.percussive ? 'P' : 'M', id.msb, id.lsb, program);

    if (program >= program_count)
        return false;

    const std::optional<unsigned> slot = find_slot(id);
    if (!slot)
        return false;

    Bank_Info &info = bank_infos_[*slot];
    if (!info.used.test(program))
        return false;

    Instrument ins;
    if (!pl_.get_instrument(info.bank, program, ins))
        return false;
    ins.blank(true);
    if (!pl_.set_instrument(info.bank, program, ins))
        return false;
    info.used.reset(program);

    if ((flags & LP_Notify) != 0)
        slots_notify_flag_ = true;
    return true;
}

bool Bank_Manager::delete_bank(const Bank_Id &id, unsigned flags)
{
    const std::optional<unsigned> slot = find_slot(id);
    if (!slot)
        return false;

    Bank_Info &info = bank_infos_[*slot];
    pl_.ensure_remove_bank(info.bank);
    forget_bank(info);

    if ((flags & LP_Notify) != 0)
        slots_notify_flag_ = true;
    return true;
}

bool Bank_Manager::load_measurement(const Bank_Id &id, unsigned program, const Instrument &ins, std::uint16_t kon, std::uint16_t koff, bool notify)
{
    trace("Loading measurement for program %c%u:%u:%u: %u ms on, %u ms off",
          id.percussive ? 'P' : 'M', id.msb, id.lsb, program, kon, koff);

    if (program >= program_count)
        return false;

    const std::optional<unsigned> slot = find_slot(id);
    if (!slot) {
        trace("The program for received measurement does not exist");
        return false;
    }

    Bank_Info &info = bank_infos_[*slot];
    Instrument current;
    if (!pl_.get_instrument(info.bank, program, current))
        return false;

    if (!ins.equal_instrument_except_delays(current)) {
        trace("The program for received measurement does not match");
        return false;
    }

    trace("The program for received measurement matches");

    current.delay_on_ms = kon;
    current.delay_off_ms = koff;
    if (!pl_.set_instrument(info.bank, program, current))
        return false;

    if (notify)
        info.to_notify.set(program);
    return true;
}

void Bank_Manager::rename_bank(const Bank_Id &id, const char *name, bool notify)
{
    const std::optional<unsigned> slot = find_slot(id);
    if (!slot)
        return;

    if (assign_name(bank_infos_[*slot].bank_name, name) && notify)
        slots_notify_flag_ = true;
}

void Bank_Manager::rename_program(const Bank_Id &id, unsigned program, const char *name, bool notify)
{
    if (program >= program_count)
        return;

    const std::optional<unsigned> slot = find_slot(id);
    if (!slot)
        return;

    Bank_Info &info = bank_infos_[*slot];
    if (assign_name(info.program_name(program), name) && notify)
        info.to_notify.set(program);
}

bool Bank_Manager::find_program(const Bank_Id &id, unsigned program, Instrument &ins)
{
    if (program >= program_count)
        return false;

    const std::optional<unsigned> slot = find_slot(id);
    if (!slot)
        return false;

    return pl_.get_instrument(bank_infos_[*slot].bank, program, ins);
}

void Bank_Manager::initialize_all_banks()
{
    Player &pl = pl_;

    trace("Update all banks");

    Bank_Ref bank;
    unsigned index = 0;

    for (bool have = pl.get_first_bank(bank); have && index < bank_reserve_size; have = pl.get_next_bank(bank)) {
        Bank_Id id;
        pl.ensure_get_bank_id(bank, id);

        trace("Update bank %c%u:%u at slot %u",
              id.percussive ? 'P' : 'M', id.msb, id.lsb, index);

        Bank_Info &info = bank_infos_[index];
        info.id = id;
        info.bank = bank;

        std::memset(info.bank_name, 0, sizeof info.bank_name);
        info.ins_names = {};

        Instrument ins;
        info.used.reset();
        for (unsigned i = 0; i < 128; ++i) {
            pl.ensure_get_instrument(bank, i, ins);
            info.used.set(i, !ins.blank());
        }

        ++index;
    }

    trace("Clear slots %u-%u", index, bank_reserve_size - 1);
    for (; index < bank_reserve_size; ++index)
        forget_bank(bank_infos_[index]);
}

// A slot that is no longer a bank holds nothing. Without this, the bits and the
// names of the bank that was there would stay until something took the slot, and
// anything that read them without asking first whether the slot is a bank would
// find programs in a bank that is not there.
void Bank_Manager::forget_bank(Bank_Info &info) noexcept
{
    info.id = Bank_Id();
    info.bank = Bank_Ref();
    info.used.reset();
    info.to_notify.reset();
    info.to_measure.reset();
    std::memset(info.bank_name, 0, sizeof info.bank_name);
    info.ins_names = {};
}

std::optional<unsigned> Bank_Manager::find_slot(const Bank_Id &id) const noexcept
{
    // A number that is not a bank names no slot. An empty slot keeps such a
    // number of its own, so without this a message could name one and rename or
    // delete the empty slot it found.
    if (!id)
        return std::nullopt;

    for (unsigned i = 0; i < bank_reserve_size; ++i) {
        if (bank_infos_[i].id == id)
            return i;
    }
    return std::nullopt;
}

std::optional<unsigned> Bank_Manager::find_empty_slot() const noexcept
{
    for (unsigned i = 0; i < bank_reserve_size; ++i) {
        const Bank_Info &info = bank_infos_[i];
        if (!info.id || info.used.none())
            return i;
    }
    return std::nullopt;
}

bool Bank_Manager::emit_slots()
{
    return Messages::send<Messages::Fx::NotifyBankSlots>(proc_.message_queue_to_ui_rt(), [this](auto &body) {
        unsigned count = 0;
        for (const Bank_Info &info : bank_infos_) {
            if (!info || info.used.none())
                continue;
            auto &entry = body.entry[count++];
            entry.bank = info.id;
            entry.used = info.used;
            static_assert(sizeof entry.name == sizeof info.bank_name);
            std::memcpy(entry.name, info.bank_name, sizeof entry.name);
        }
        body.count = count;
    });
}

bool Bank_Manager::emit_notification(const Bank_Info &info, unsigned program)
{
    return Messages::send<Messages::Fx::NotifyInstrument>(proc_.message_queue_to_ui_rt(), [&](auto &body) {
        body.bank = info.id;
        body.program = static_cast<std::uint8_t>(program);
        pl_.ensure_get_instrument(info.bank, program, body.instrument);
        static_assert(sizeof body.instrument.name == name_size);
        std::ranges::copy(info.program_name(program), std::begin(body.instrument.name));
    });
}

bool Bank_Manager::emit_measurement_request(const Bank_Info &info, unsigned program)
{
    const bool sent = Messages::send<Messages::Fx::RequestMeasurement>(proc_.message_queue_to_worker(), [&](auto &body) {
        body.bank = info.id;
        body.program = static_cast<std::uint8_t>(program);
        pl_.ensure_get_instrument(info.bank, program, body.instrument);
        std::memset(body.instrument.name, 0, sizeof body.instrument.name);
    });
    if (sent)
        proc_.worker()->postSemaphore();
    return sent;
}
