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
#include "adl/instrument.h"
#include "adl/chip_settings.h"
#include "utility/simple_fifo.h"
#include "utility/counting_bitset.h"
#include "definitions.h"
#include <algorithm>
#include <bitset>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>
#include <utility>

enum class User_Message : unsigned;
enum class Fx_Message : unsigned;
enum class Worker_Message : unsigned;

struct Message_Header {
    unsigned tag = 0;
    unsigned size = 0;
};

// A message in a Simple_Fifo, as read or as reserved for writing: a header,
// then `header.size` bytes of body. The header and the body are copied in and
// out of the FIFO's bytes, never used where they lie, so neither the type of
// what is there nor its alignment comes into it.
struct Buffered_Message {
    Message_Header header;
    std::span<std::uint8_t> body;
    // The bytes that the whole message takes in the FIFO.
    unsigned length = 0;
    bool valid = false;
    explicit operator bool() const noexcept
        { return valid; }
};

namespace Messages {
    // A body is a struct that is copied as its bytes, with its tag.
    template <class T>
    concept Body = std::is_trivially_copyable_v<T> && std::is_default_constructible_v<T> &&
                   std::is_enum_v<std::remove_cv_t<decltype(T::tag)>>;

    Buffered_Message read(Simple_Fifo &fifo) noexcept;
    void finish_read(Simple_Fifo &fifo, const Buffered_Message &msg) noexcept;

    // Reserves a message with a body of `size` bytes, which are filled in
    // before finish_write().
    Buffered_Message write(Simple_Fifo &fifo, unsigned tag, unsigned size) noexcept;
    void finish_write(Simple_Fifo &fifo, const Buffered_Message &msg) noexcept;

    // Reserves a message whose body is a T.
    template <Body T>
    Buffered_Message write(Simple_Fifo &fifo) noexcept
        { return write(fifo, std::to_underlying(T::tag), sizeof(T)); }

    // A copy of the body of a message that carries a T. No more than the body
    // is read, whatever its size.
    template <Body T>
    T body(const Buffered_Message &msg) noexcept
    {
        assert(msg.header.tag == std::to_underlying(T::tag) && msg.body.size() == sizeof(T));
        T value {};
        std::memcpy(&value, msg.body.data(), std::min(msg.body.size(), sizeof value));
        return value;
    }

    // Copies a T into the body of a message reserved for it. No more than the
    // body is written, whatever its size.
    template <Body T>
    void set_body(const Buffered_Message &msg, const T &value) noexcept
    {
        assert(msg.header.tag == std::to_underlying(T::tag) && msg.body.size() == sizeof(T));
        std::memcpy(msg.body.data(), &value, std::min(msg.body.size(), sizeof value));
    }

    // Sends a T filled in by `fill(body)`; false if the FIFO has no room.
    template <Body T, class Fill>
    bool send(Simple_Fifo &fifo, Fill &&fill)
    {
        const Buffered_Message msg = write<T>(fifo);
        if (!msg)
            return false;
        T value {};
        std::forward<Fill>(fill)(value);
        set_body(msg, value);
        finish_write(fifo, msg);
        return true;
    }
}  // namespace Messages

//------------------------------------------------------------------------------
enum class User_Message : unsigned {
    Midi = 0x1000,  // midi event
    RequestBankSlots,  // requests the layout of banks and instruments
    RequestFullBankState,  // requests all the managed bank state
    RequestChipSettings,  // requests chip settings
    RequestSelections,  // requests selections
    RequestActivePart,  // requests the active part
    RequestBankTitle,  // requests the bank title
    ClearBanks,  // deletes all the managed banks
    LoadGlobalParameters,  // edits global parameters
    LoadInstrument,  // edits an instrument
    CreateInstrument,  // creates an instrument
    DeleteInstrument,  // deletes an instrument
    DeleteBank,  // deletes a bank
    RenameBank,  // set bank name
    RenameProgram,  // set program name
    SelectProgram,  // changes selected program number
    SetActivePart,  // sets the active part
    SetBankTitle,  // sets the bank title
#if defined(ADLPLUG_OPL3)
    SelectOptimal4Ops,  // sets the optimal 4op channel count
#endif
};

namespace Messages::User {

struct RequestBankSlots {
    static constexpr User_Message tag = User_Message::RequestBankSlots;
};

struct RequestFullBankState {
    static constexpr User_Message tag = User_Message::RequestFullBankState;
};

struct RequestChipSettings {
    static constexpr User_Message tag = User_Message::RequestChipSettings;
};

struct RequestSelections {
    static constexpr User_Message tag = User_Message::RequestSelections;
    std::bitset<16> channel_mask;
};

struct RequestActivePart {
    static constexpr User_Message tag = User_Message::RequestActivePart;
};

struct RequestBankTitle {
    static constexpr User_Message tag = User_Message::RequestBankTitle;
};

struct ClearBanks {
    static constexpr User_Message tag = User_Message::ClearBanks;
    bool notify_back = false;
};

struct LoadGlobalParameters {
    static constexpr User_Message tag = User_Message::LoadGlobalParameters;
    Instrument_Global_Parameters param;
    bool notify_back = false;
};

struct LoadInstrument {
    static constexpr User_Message tag = User_Message::LoadInstrument;
    unsigned part = 0;
    Bank_Id bank;
    std::uint8_t program = 0;
    Instrument instrument;
    bool need_measurement = false;
    bool notify_back = false;
};

struct CreateInstrument {
    static constexpr User_Message tag = User_Message::CreateInstrument;
    Bank_Id bank;
    std::uint8_t program = 0;
    bool notify_back = false;
};

struct DeleteInstrument {
    static constexpr User_Message tag = User_Message::DeleteInstrument;
    Bank_Id bank;
    std::uint8_t program = 0;
    bool notify_back = false;
};

struct DeleteBank {
    static constexpr User_Message tag = User_Message::DeleteBank;
    Bank_Id bank;
    bool notify_back = false;
};

struct RenameBank {
    static constexpr User_Message tag = User_Message::RenameBank;
    Bank_Id bank;
    bool notify_back = false;
    char name[32] {};
};

struct RenameProgram {
    static constexpr User_Message tag = User_Message::RenameProgram;
    Bank_Id bank;
    std::uint8_t program = 0;
    bool notify_back = false;
    char name[32] {};
};

struct SelectProgram {
    static constexpr User_Message tag = User_Message::SelectProgram;
    unsigned part = 0;
    Bank_Id bank;
    std::uint8_t program = 0;
};

struct SetActivePart {
    static constexpr User_Message tag = User_Message::SetActivePart;
    unsigned part = 0;
};

struct SetBankTitle {
    static constexpr User_Message tag = User_Message::SetBankTitle;
    char title[64] {};
};

#if defined(ADLPLUG_OPL3)
struct SelectOptimal4Ops {
    static constexpr User_Message tag = User_Message::SelectOptimal4Ops;
};
#endif

}  // namespace Messages::User

//------------------------------------------------------------------------------
enum class Fx_Message : unsigned {
    NotifyReady = 0x2000,  // notifies readiness
    NotifyBankSlots,  // notifies the layout of banks and instruments
    NotifyGlobalParameters,  // notifies the global parameters
    NotifyInstrument,  // notifies an instrument when changed
    NotifyChipSettings,  // notifies chip settings when changed
    NotifySelection,  // notifies selection when changed
    NotifyActivePart,  // notifies active part when changed
    NotifyBankTitle,  // notifies bank title when changed
    RequestMeasurement,  // request measurement of a program
    RequestChipSettings,  // request a change of chip settings
};

namespace Messages::Fx {

struct NotifyReady {
    static constexpr Fx_Message tag = Fx_Message::NotifyReady;
};

struct NotifyBankSlots {
    static constexpr Fx_Message tag = Fx_Message::NotifyBankSlots;
    struct Entry {
        Bank_Id bank;
        counting_bitset<128> used;
        char name[32] {};
    };
    unsigned count = 0;
    Entry entry[bank_reserve_size];
};

struct NotifyGlobalParameters {
    static constexpr Fx_Message tag = Fx_Message::NotifyGlobalParameters;
    Instrument_Global_Parameters param;
};

struct NotifyInstrument {
    static constexpr Fx_Message tag = Fx_Message::NotifyInstrument;
    Bank_Id bank;
    std::uint8_t program = 0;
    Instrument instrument;
};

struct NotifyChipSettings {
    static constexpr Fx_Message tag = Fx_Message::NotifyChipSettings;
    Chip_Settings cs;
};

struct NotifySelection {
    static constexpr Fx_Message tag = Fx_Message::NotifySelection;
    unsigned part = 0;
    Bank_Id bank;
    std::uint8_t program = 0;
};

struct NotifyActivePart {
    static constexpr Fx_Message tag = Fx_Message::NotifyActivePart;
    unsigned part = 0;
};

struct NotifyBankTitle {
    static constexpr Fx_Message tag = Fx_Message::NotifyBankTitle;
    char title[64] {};
};

struct RequestMeasurement {
    static constexpr Fx_Message tag = Fx_Message::RequestMeasurement;
    Bank_Id bank;
    std::uint8_t program = 0;
    Instrument instrument;
};

struct RequestChipSettings {
    static constexpr Fx_Message tag = Fx_Message::RequestChipSettings;
    Chip_Settings cs;
};

}  // namespace Messages::Fx

//------------------------------------------------------------------------------
enum class Worker_Message : unsigned {
    MeasurementResult = 0x3000,  // result of a measurement operation
};

namespace Messages::Worker {

struct MeasurementResult {
    static constexpr Worker_Message tag = Worker_Message::MeasurementResult;
    Bank_Id bank;
    std::uint8_t program = 0;
    Instrument instrument;
    std::uint16_t ms_sound_kon = 0;
    std::uint16_t ms_sound_koff = 0;
};

}  // namespace Messages::Worker
