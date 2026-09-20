// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// What a user's file becomes: the bytes of a bank file or an instrument file,
// read into what the plugin holds, and then the messages that put it there. Both
// halves used to be inside the editor (Generic_Main_Component::load_bank_mem),
// where nothing but the editor could reach them, so the bytes of a file and the
// messages about them were fuzzed at their two ends and never in one piece.
// Neither half needs the interface, and here they need no more than the messages
// themselves do.
//
// Whoever has bytes reads them the same way: the editor, which took them from a
// file the user chose or from the pack the plugin carries, and the fuzz target
// for the messages the editor sends, which makes them up.

#pragma once
#include "messages.h"
#include "adl/instrument.h"
#include "utility/name_field.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

// What the bytes of a bank file say, once the library has read them: the banks
// with their instruments and the names the file gave them, the parameters that
// belong to the whole file, whether the instruments have still to be measured (a
// bank file of the library's own format carries the times), and the chip that the
// file names, which is a parameter of the plugin rather than anything a message
// carries (OPN2 only).
struct Bank_File_Contents {
    std::vector<Midi_Bank> banks;
    Instrument_Global_Parameters global;
    bool need_measurement = true;
    std::optional<int> chip_type;
};

// Reads the bytes of a bank file, and answers nothing when they are not one. The
// library's reader takes memory it may write to, although it only reads, so the
// bytes are not const: whoever has them holds them where they can be written,
// rather than casting that away (the editor reads a file into its own buffer, and
// a fuzz target copies the bytes it is lent).
std::optional<Bank_File_Contents> read_bank_file(std::span<std::uint8_t> data);

// Reads the bytes of one instrument: the library's own format, or, where the
// chip has one, the other format the editor offers (SBI on OPL3, which is
// `format` 1).
std::optional<Instrument> read_instrument_file(std::span<std::uint8_t> data, int format);

// The messages the editor sends about a bank file it has read, in the order it
// sends them: the title, the parameters of the file, clearing the banks, an
// instrument for every program of every bank, a name for every bank, and the two
// requests that ask for the result.
//
// `send` is the caller's own way of sending one message, because the ways differ
// where it matters: the editor keeps what the queue cannot take and sends it
// again on a timer (Generic_Main_Component::write_to_processor), and a test
// writes straight to the queue. What is sent, and in what order, is the same
// either way.
template <class Send>
void send_bank_file(Send &&send, const Bank_File_Contents &contents,
                    const juce::String &title, unsigned part)
{
    {
        Messages::User::SetBankTitle msg;
        copy_name_to_field(msg.title, title);
        send(msg);
    }

    {
        Messages::User::LoadGlobalParameters msg;
        msg.param = contents.global;
        msg.notify_back = true;
        send(msg);
    }

    {
        Messages::User::ClearBanks msg;
        msg.notify_back = false;
        send(msg);
    }

    for (const Midi_Bank &bank : contents.banks) {
        for (std::size_t i = 0; i < bank.ins.size(); ++i) {
            Messages::User::LoadInstrument msg;
            msg.part = part;
            msg.bank = bank.id;
            msg.program = static_cast<std::uint8_t>(i);
            msg.instrument = bank.ins[i];
            msg.need_measurement = contents.need_measurement;
            msg.notify_back = false;
            send(msg);
        }

        Messages::User::RenameBank msg;
        msg.bank = bank.id;
        msg.notify_back = false;
        static_assert(sizeof msg.name == sizeof bank.name);
        std::memcpy(msg.name, bank.name, sizeof msg.name);
        send(msg);
    }

    send(Messages::User::RequestFullBankState{});
    send(Messages::User::RequestBankTitle{});
}

// And the one message the editor sends about an instrument file. Which bank and
// which program it goes into is the caller's: the editor keeps its selection as
// one number, of which the bank is the upper bits and the percussive half is bit
// 7, and that reading belongs to the editor.
template <class Send>
void send_instrument_file(Send &&send, const Instrument &instrument,
                          Bank_Id bank, std::uint8_t program, unsigned part)
{
    Messages::User::LoadInstrument msg;
    msg.part = part;
    msg.bank = bank;
    msg.program = program;
    msg.instrument = instrument;
    msg.need_measurement = true;
    msg.notify_back = true;
    send(msg);
}
