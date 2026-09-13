//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

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

// Stores `name` in a name field, leaving out the characters which do not fit
// whole. Returns false if the field held it already.
bool assign_name(char *field, const char *name) noexcept
{
    constexpr std::size_t size = Bank_Manager::name_size;
    char stored[size] {};
    std::memcpy(stored, name, utf8_fitting_length(name, size));
    if (std::memcmp(field, stored, size) == 0)
        return false;
    std::memcpy(field, stored, size);
    return true;
}

}  // namespace

Bank_Manager::Bank_Manager(AdlplugAudioProcessor &proc, Player &pl, const void *wopl_data, std::size_t wopl_size)
    : proc_(proc), pl_(pl)
{
    WOPx::BankFile_Ptr wopl;
    if (pl.load_bank_data(wopl_data, wopl_size))
        wopl.reset(WOPx::LoadBankFromMem(const_cast<void *>(wopl_data), wopl_size, nullptr));

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
        info.id = Bank_Id();
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

        info.id = id;
        pl.ensure_get_bank(id, Player::Bank_CreateRt, info.bank);
        info.used.reset();
        info.to_notify.reset();
        info.to_measure.reset();
        std::memset(info.bank_name, 0, sizeof info.bank_name);
        std::memset(info.ins_names, 0, sizeof info.ins_names);
    }

    Bank_Info &info = bank_infos_[*slot];

    Instrument old_ins;
    pl.ensure_get_instrument(info.bank, program, old_ins);

    const bool replace = (flags & LP_NoReplaceExisting) == 0 || old_ins.blank();
    if (!replace)
        return false;

    pl.ensure_set_instrument(info.bank, program, ins);

    // copy name
    if ((flags & LP_KeepName) == 0)
        std::memcpy(info.program_name(program), ins.name, name_size);

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

    const std::optional<unsigned> slot = find_slot(id);
    if (!slot)
        return false;

    Bank_Info &info = bank_infos_[*slot];
    if (!info.used.test(program))
        return false;

    Instrument ins;
    pl_.ensure_get_instrument(info.bank, program, ins);
    ins.blank(true);
    pl_.ensure_set_instrument(info.bank, program, ins);
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
    info.id = Bank_Id();

    if ((flags & LP_Notify) != 0)
        slots_notify_flag_ = true;
    return true;
}

bool Bank_Manager::load_measurement(const Bank_Id &id, unsigned program, const Instrument &ins, std::uint16_t kon, std::uint16_t koff, bool notify)
{
    trace("Loading measurement for program %c%u:%u:%u: %u ms on, %u ms off",
          id.percussive ? 'P' : 'M', id.msb, id.lsb, program, kon, koff);

    const std::optional<unsigned> slot = find_slot(id);
    if (!slot) {
        trace("The program for received measurement does not exist");
        return false;
    }

    Bank_Info &info = bank_infos_[*slot];
    Instrument current;
    pl_.ensure_get_instrument(info.bank, program, current);

    if (!ins.equal_instrument_except_delays(current)) {
        trace("The program for received measurement does not match");
        return false;
    }

    trace("The program for received measurement matches");

    current.delay_on_ms = kon;
    current.delay_off_ms = koff;
    pl_.ensure_set_instrument(info.bank, program, current);

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
    const std::optional<unsigned> slot = find_slot(id);
    if (!slot)
        return;

    Bank_Info &info = bank_infos_[*slot];
    if (assign_name(info.program_name(program), name) && notify)
        info.to_notify.set(program);
}

bool Bank_Manager::find_program(const Bank_Id &id, unsigned program, Instrument &ins)
{
    const std::optional<unsigned> slot = find_slot(id);
    if (!slot)
        return false;

    pl_.ensure_get_instrument(bank_infos_[*slot].bank, program, ins);
    return true;
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
        std::memset(info.ins_names, 0, sizeof info.ins_names);

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
        bank_infos_[index].id = Bank_Id();
}

std::optional<unsigned> Bank_Manager::find_slot(const Bank_Id &id) const noexcept
{
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
        std::memcpy(body.instrument.name, info.program_name(program), sizeof body.instrument.name);
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
