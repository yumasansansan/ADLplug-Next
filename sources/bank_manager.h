//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

#pragma once
#include "definitions.h"
#include "adl/instrument.h"
#include "utility/counting_bitset.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
class AdlplugAudioProcessor;
class Player;

// Keeps track of the banks loaded in the player, the names of banks and
// programs (which the player does not store), and which programs need to be
// sent to the editor or measured by the worker. Runs on the audio thread.
class Bank_Manager {
public:
    Bank_Manager(AdlplugAudioProcessor &proc, Player &pl, const void *wopl_data, std::size_t wopl_size);
    void clear_banks(bool notify);

    void mark_everything_for_notification();
    void mark_slots_for_notification();
    void send_notifications();
    void send_measurement_requests();

    bool load_program(const Bank_Id &id, unsigned program, const Instrument &ins, unsigned flags);
    bool delete_program(const Bank_Id &id, unsigned program, unsigned flags);
    bool find_program(const Bank_Id &id, unsigned program, Instrument &ins);
    bool delete_bank(const Bank_Id &id, unsigned flags);

    bool load_measurement(const Bank_Id &id, unsigned program, const Instrument &ins, std::uint16_t kon, std::uint16_t koff, bool notify);

    void rename_bank(const Bank_Id &id, const char *name, bool notify);
    void rename_program(const Bank_Id &id, unsigned program, const char *name, bool notify);

    enum : unsigned {
        LP_Notify            = 1,
        LP_NeedMeasurement   = 2,
        LP_KeepName          = 4,
        LP_NoReplaceExisting = 8,
    };

    // Names are fixed-size fields, not necessarily terminated.
    static constexpr std::size_t name_size = 32;

    struct Bank_Info {
        Bank_Id id;
        Bank_Ref bank;
        counting_bitset<128> used;
        counting_bitset<128> to_notify;
        counting_bitset<128> to_measure;
        char bank_name[name_size] {};
        char ins_names[128 * name_size] {};

        explicit operator bool() const noexcept
            { return static_cast<bool>(id); }
        char *program_name(unsigned program) noexcept
            { return &ins_names[name_size * program]; }
        const char *program_name(unsigned program) const noexcept
            { return &ins_names[name_size * program]; }
    };

    const std::array<Bank_Info, bank_reserve_size> &bank_infos() const noexcept
        { return bank_infos_; }

private:
    void initialize_all_banks();

    std::optional<unsigned> find_slot(const Bank_Id &id) const noexcept;
    std::optional<unsigned> find_empty_slot() const noexcept;

    bool emit_slots();
    bool emit_notification(const Bank_Info &info, unsigned program);
    bool emit_measurement_request(const Bank_Info &info, unsigned program);

    AdlplugAudioProcessor &proc_;
    Player &pl_;
    std::array<Bank_Info, bank_reserve_size> bank_infos_;
    bool slots_notify_flag_ = false;
};
