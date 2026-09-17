// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

#include "test.h"
#include "messages.h"
#include "utility/name_field.h"
#include "utility/simple_fifo.h"
#include <algorithm>
#include <array>
#include <bitset>
#include <cstdint>
#include <cstring>
#include <format>
#include <utility>

ADLPLUG_TEST(simple_fifo_round_trip)
{
    // Messages of changing lengths go around the ring buffer many times. Each
    // read has to see its message in one piece, also when it was written
    // across the end of the buffer.
    Simple_Fifo fifo(64);
    std::uint8_t next = 0;
    for (unsigned round = 0; round < 256; ++round) {
        const unsigned length = 1 + round % 48;

        unsigned write_offset = 0;
        std::uint8_t *const written = fifo.write(length, write_offset);
        CHECK(written != nullptr);
        if (written == nullptr)
            return;
        for (unsigned i = 0; i < length; ++i)
            written[i] = static_cast<std::uint8_t>(next + i);
        fifo.finish_write(write_offset);

        unsigned read_offset = 0;
        const std::uint8_t *const message = fifo.read(length, read_offset);
        CHECK(message != nullptr);
        if (message == nullptr)
            return;
        bool same = true;
        for (unsigned i = 0; i < length; ++i)
            same = same && message[i] == static_cast<std::uint8_t>(next + i);
        CHECK(same);
        CHECK(read_offset == write_offset);
        fifo.finish_read(read_offset);
        CHECK(fifo.get_num_ready() == 0);

        next = static_cast<std::uint8_t>(next + length);
    }
}

ADLPLUG_TEST(simple_fifo_full)
{
    // AbstractFifo keeps one byte of its capacity free, so a FIFO of 64 bytes
    // takes three messages of 16, and refuses a fourth.
    Simple_Fifo fifo(64);
    unsigned messages = 0;
    for (;;) {
        unsigned offset = 0;
        if (fifo.write(16, offset) == nullptr)
            break;
        fifo.finish_write(offset);
        ++messages;
    }
    CHECK(messages == 3);
    CHECK(fifo.get_num_ready() == 48);
}

ADLPLUG_TEST(simple_fifo_too_long)
{
    // A length that the FIFO could never hold is refused, also one that would
    // wrap around to a small number when added to the offset.
    Simple_Fifo fifo(64);
    unsigned offset = 8;
    CHECK(fifo.write(0xfffffff8u, offset) == nullptr);
    CHECK(fifo.read(0xfffffff8u, offset) == nullptr);
    CHECK(fifo.write(65, offset) == nullptr);
    CHECK(offset == 8);
}

namespace {

Instrument test_instrument(unsigned seed)
{
    Instrument ins;
    ins.blank(false);
    ins.percussion_key_number = static_cast<std::uint8_t>(seed % 128);
    ins.delay_on_ms = static_cast<std::uint16_t>(seed * 11);
    ins.delay_off_ms = static_cast<std::uint16_t>(seed * 7);
    for (unsigned op = 0; op < 4; ++op) {
        ins.level(op, static_cast<int>((seed + op) % 32));
        ins.fmul(op, static_cast<int>((3 * seed + op) % 16));
    }
    copy_name_to_field(ins.name, juce::String(std::format("Instrument {}", seed)));
    return ins;
}

bool same_instrument(const Instrument &a, const Instrument &b)
{
    return a.equal_instrument(b) && std::ranges::equal(a.name, b.name);
}

Messages::Fx::NotifyBankSlots test_slots(unsigned seed)
{
    Messages::Fx::NotifyBankSlots slots;
    slots.count = seed % (bank_reserve_size + 1);
    for (unsigned i = 0; i < slots.count; ++i) {
        auto &entry = slots.entry[i];
        entry.bank = Bank_Id(static_cast<std::uint8_t>(i % 128), static_cast<std::uint8_t>(seed % 128), (i & 1) != 0);
        for (unsigned program = 0; program < 128; program += 1 + (i + seed) % 7)
            entry.used.set(program);
        copy_name_to_field(entry.name, juce::String(std::format("Bank {} of {}", i, seed)));
    }
    return slots;
}

bool same_slots(const Messages::Fx::NotifyBankSlots &a, const Messages::Fx::NotifyBankSlots &b)
{
    if (a.count != b.count)
        return false;
    for (unsigned i = 0; i < a.count; ++i) {
        const auto &x = a.entry[i];
        const auto &y = b.entry[i];
        if (!(x.bank == y.bank) || !(x.used == y.used) || !std::ranges::equal(x.name, y.name))
            return false;
    }
    return true;
}

}  // namespace

ADLPLUG_TEST(messages_round_trip)
{
    // The messages of the editor, the processor and the worker go through a
    // queue as their bytes, across the end of its buffer too, and come out as
    // they went in: the largest one (the slots of the banks), one that holds an
    // instrument, one that holds a bitset, and raw MIDI.
    Simple_Fifo fifo(16 * 1024);
    for (unsigned round = 0; round < 64; ++round) {
        Messages::User::LoadInstrument load;
        load.part = round % 16;
        load.bank = Bank_Id(static_cast<std::uint8_t>(round % 128), 3, (round & 2) != 0);
        load.program = static_cast<std::uint8_t>((5 * round) % 128);
        load.instrument = test_instrument(round);
        load.need_measurement = (round & 1) != 0;
        load.notify_back = (round & 4) != 0;

        const Messages::Fx::NotifyBankSlots slots = test_slots(round);

        Messages::User::RequestSelections selections;
        selections.channel_mask = std::bitset<16>(0x9e37u * (round + 1));

        const std::array<std::uint8_t, 3> midi {0x90, static_cast<std::uint8_t>(round % 128), 100};

        CHECK(Messages::send<Messages::User::LoadInstrument>(fifo, [&load](auto &body) { body = load; }));
        CHECK(Messages::send<Messages::Fx::NotifyBankSlots>(fifo, [&slots](auto &body) { body = slots; }));
        CHECK(Messages::send<Messages::User::RequestSelections>(fifo, [&selections](auto &body) { body = selections; }));
        const Buffered_Message raw =
            Messages::write(fifo, std::to_underlying(User_Message::Midi), static_cast<unsigned>(midi.size()));
        CHECK(raw && raw.body.size() == midi.size());
        if (!raw)
            return;
        std::ranges::copy(midi, raw.body.begin());
        Messages::finish_write(fifo, raw);

        Buffered_Message msg = Messages::read(fifo);
        CHECK(msg && msg.header.tag == std::to_underlying(User_Message::LoadInstrument));
        if (!msg)
            return;
        const auto load_received = Messages::body<Messages::User::LoadInstrument>(msg);
        CHECK(load_received.part == load.part && load_received.bank == load.bank &&
              load_received.program == load.program && same_instrument(load_received.instrument, load.instrument) &&
              load_received.need_measurement == load.need_measurement && load_received.notify_back == load.notify_back);
        Messages::finish_read(fifo, msg);

        msg = Messages::read(fifo);
        CHECK(msg && msg.header.tag == std::to_underlying(Fx_Message::NotifyBankSlots));
        if (!msg)
            return;
        CHECK(same_slots(Messages::body<Messages::Fx::NotifyBankSlots>(msg), slots));
        Messages::finish_read(fifo, msg);

        msg = Messages::read(fifo);
        CHECK(msg && msg.header.tag == std::to_underlying(User_Message::RequestSelections));
        if (!msg)
            return;
        CHECK(Messages::body<Messages::User::RequestSelections>(msg).channel_mask == selections.channel_mask);
        Messages::finish_read(fifo, msg);

        msg = Messages::read(fifo);
        CHECK(msg && msg.header.tag == std::to_underlying(User_Message::Midi));
        if (!msg)
            return;
        CHECK(std::ranges::equal(msg.body, midi));
        Messages::finish_read(fifo, msg);
    }
    CHECK(fifo.get_num_ready() == 0);
}

ADLPLUG_TEST(messages_reserved_then_filled)
{
    // The worker reserves room for its result before it measures, and fills it
    // in afterwards. Until the message is finished, there is nothing to read.
    Simple_Fifo fifo(1024);
    Messages::Worker::MeasurementResult result;
    result.bank = Bank_Id(5, 7, true);
    result.program = 42;
    result.instrument = test_instrument(42);
    result.ms_sound_kon = 1234;
    result.ms_sound_koff = 567;

    const Buffered_Message reserved = Messages::write<Messages::Worker::MeasurementResult>(fifo);
    CHECK(reserved);
    if (!reserved)
        return;
    Messages::set_body(reserved, result);
    CHECK(!Messages::read(fifo));
    Messages::finish_write(fifo, reserved);

    const Buffered_Message msg = Messages::read(fifo);
    CHECK(msg && msg.header.tag == std::to_underlying(Worker_Message::MeasurementResult));
    if (!msg)
        return;
    const auto received = Messages::body<Messages::Worker::MeasurementResult>(msg);
    CHECK(received.bank == result.bank && received.program == result.program &&
          same_instrument(received.instrument, result.instrument) &&
          received.ms_sound_kon == result.ms_sound_kon && received.ms_sound_koff == result.ms_sound_koff);
    Messages::finish_read(fifo, msg);
    CHECK(fifo.get_num_ready() == 0);
}

ADLPLUG_TEST(messages_bad_size)
{
    // A header whose size is more than the queue could hold does not make the
    // body run past the buffer: the message is not read at all.
    Simple_Fifo fifo(64);
    const Message_Header header {std::to_underlying(User_Message::Midi), 0xffffffffu};
    unsigned offset = 0;
    std::uint8_t *const bytes = fifo.write(sizeof header, offset);
    CHECK(bytes != nullptr);
    if (bytes == nullptr)
        return;
    std::memcpy(bytes, &header, sizeof header);
    fifo.finish_write(offset);
    CHECK(!Messages::read(fifo));
}
