// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// What the editor asks the plugin to do with its banks, one message after
// another. The editor never touches a bank itself: it writes a message into a
// queue -- load this instrument, make one, delete one, delete the bank, rename
// either, select a program -- and the processor hands it to the bank manager on
// the audio thread, which keeps the sixty-four slots, the names that the library
// does not store, and the bits that say which programs are in use, which are to
// be sent to the editor and which are to be measured. A project restored by a
// host takes the same road (plugin_processor.cc reads the state into the same
// calls), so what the messages can reach, a project can reach.
//
// A file the user opens is the other way in, and this target takes one whole:
// the bytes go through the library's reader and the editor's own way of turning
// what it found into these messages (sources/bank_load.h), which is the piece
// between fuzz/bank_file.cc, where a file's bytes stop at the reader, and the
// records here, where the messages start with values of their own.
//
// The input is a list of records; there is no header. A record is one byte, the
// top four bits saying what it is and the low four carrying what fits, and the
// bytes after it carry the rest:
//
//   0   a block of audio of 1 + v * 4 samples, which is where the processor
//       takes the messages, sends the notifications and applies the parameter
//       changes; the slots are checked after it
//   1   ask the plugin for something: the layout of the banks, everything, the
//       chip settings, the selections (a mask of two bytes), the active part or
//       the title of the bank
//   2   load an instrument into a bank and a program (four bytes: the two halves
//       of the bank's number, the program, and what else the message carries)
//   3   make an instrument, 4 delete one, 5 delete the bank, 6 rename the bank,
//       7 rename a program, 8 select a program, 9 set the active part (a byte),
//       10 set the title of the bank, 11 clear every bank
//   12  a measurement as the worker sends one back, of the instrument the plugin
//       holds or of another, with the two times it found (two bytes each)
//   13  the global parameters (two bytes: the volume model, and what else is
//       global to the chip), 14 prepare the plugin again (once at most)
//   15  with the low bit clear, the best number of four-operator channels (OPL3;
//       nothing on OPN2). With it set, the rest of the input as the bytes of a
//       file the user opens, read and sent the way the editor does it: a bank
//       file, or, with bit 2, an instrument file for the bank and the program
//       that follow -- in the other format of one instrument with bit 3, which is
//       SBI on OPL3. An input that begins as one of the library's files is such a
//       file whole, with no record at all, which makes every bank of the
//       submodules a seed of this target
//
// An instrument is one of four: blank, not blank and otherwise empty, one whose
// first bytes come from the input, or the one the plugin already holds there.
// Which matters here is whether it is blank, since that is what a program being
// in use means.
//
// What is checked, besides the sanitizers: that no slot holds programs without
// being a bank the plugin can show (the editor's list, the state of a project
// and the search for a free slot all pass such a slot by), that no two slots are
// the same bank, that the count a slot keeps is the number of its bits, that a
// program is in use exactly when the instrument in it is not blank, that what
// the plugin tells the editor is what the plugin holds, and that preparing the
// plugin again -- which writes the state and reads it back -- leaves the state
// it would save unchanged.
//
// A name is any bytes, as a bank file's name field is, and may fill its field
// with no terminator left: what the plugin makes of such bytes is part of what is
// checked, since it writes them into the state of a project.
//
// The measurement itself is fuzz/measurement.cc's, so this target plays the
// worker as well as the editor: it sends the answer a measurement comes back as,
// which puts the delays in. The answer goes in the editor's queue, because the
// queue the worker writes has a writer of its own already, and the processor
// handles it the same way. The real worker is still there, and a record that asks
// for a measurement has it play the instrument for as long as forty seconds; an
// input pays for the one that is running when it ends.

#include "fuzz.h"
#include "plugin_processor.h"
#include "bank_manager.h"
#include "bank_load.h"
#include "messages.h"
#include "definitions.h"
#include "adl/instrument.h"
#include "JuceHeader.h"
#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

// The structure the library keeps an instrument in, which the input fills.
#if defined(ADLPLUG_OPL3)
using Native_Instrument = ADL_Instrument;
#elif defined(ADLPLUG_OPN2)
using Native_Instrument = OPN2_Instrument;
#endif

// What one input may ask for. These bound the time one input may take, not what
// the plugin is expected to stand: every message is of a fixed size and the
// plugin sees each one on its own, and 512 records are more than the sixty-four
// slots can be filled with. The one lump of any size that belongs here is a file
// the user opens, because what the editor does with one is send these messages;
// the state of a project and the configuration belong to the targets that take
// those.
//
// Preparing the plugin again writes its state three times and reads it once,
// and the default bank alone makes that state large: an input that prepared
// four times took 13 seconds under the address sanitizer on a desktop, and 63
// under the memory sanitizer on CI, past the minute an input may take. One is
// enough for what the record is there to reach: the plugin is prepared before
// the first record, so the one is a prepare after a prepare, which carries the
// state over.
constexpr unsigned records_max = 512;
constexpr unsigned prepares_max = 1;

// How many instruments the check that reads every program may look at while the
// records run; the check after the last record reads them all.
constexpr unsigned lookups_max = 8192;

// How many times one message of a file is offered to the queue, a block of audio
// apart: the processor takes what is there in one block, so the first of these is
// nearly always enough.
constexpr unsigned send_tries = 8;

// The blocks in which the notifications of the whole state come back at the end,
// and the blocks before them, in which the parameters settle what they owe.
constexpr unsigned settle_blocks = 2;
constexpr unsigned notify_blocks = 8;

// The audio is not what this target is about: one rate, and blocks small enough
// that the samples cost nothing.
constexpr double rate = 44100.0;
constexpr int block_size = 64;

// The input, as the records read it. Past the end it is zeroes.
class Input {
public:
    Input(const std::uint8_t *data, std::size_t size) noexcept
        : data_(data), size_(size) {}

    [[nodiscard]] bool done() const noexcept
        { return at_ >= size_; }

    std::uint8_t byte() noexcept
        { return (at_ < size_) ? data_[at_++] : 0; }

    // The rest of the input, whatever it is, and nothing after it: what a record
    // hands over as one lump, as the bytes of a file are.
    std::span<const std::uint8_t> rest() noexcept
    {
        const std::size_t from = std::min(at_, size_);
        at_ = size_;
        return {data_ + from, size_ - from};
    }

    // The next `count` bytes, whatever they are.
    std::vector<char> bytes(std::size_t count)
    {
        std::vector<char> out;
        out.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
            out.push_back(static_cast<char>(byte()));
        return out;
    }

    // The next two bytes as one number, the first of them the higher. They are
    // read one after the other rather than in one expression, where nothing
    // would say which of them comes first.
    unsigned two_bytes() noexcept
    {
        const unsigned high = byte();
        const unsigned low = byte();
        return high << 8 | low;
    }

private:
    const std::uint8_t *data_;
    std::size_t size_;
    std::size_t at_ = 0;
};

// The bank a record names: the two halves of a MIDI bank number, and whether it
// is the percussive one. A half above 127 is no bank the libraries can hold, and
// the plugin has to say so rather than ask for one.
Bank_Id bank_from(Input &input, bool percussive)
{
    const std::uint8_t msb = input.byte();
    const std::uint8_t lsb = input.byte();
    return Bank_Id(msb, lsb, percussive);
}

// The instrument a record asks for: `choice` says which, and `held` is the one
// the plugin has in that program, if it has one.
Instrument instrument_from(Input &input, unsigned choice, const Instrument &held)
{
    switch (choice & 3u) {
    case 0:
        // Blank: a program loaded with this one is not in use.
        return Instrument();
    case 1: {
        Instrument ins;
        ins.blank(false);
        return ins;
    }
    case 2: {
        // The instrument as the input has it, every byte of the structure the
        // library keeps one in, which is what a bank file fills: the version, the
        // note offsets, the key a percussion instrument plays, the flags with the
        // blank bit among them, and the registers. The version is worth reaching
        // -- the library takes the one it knows and refuses any other -- since a
        // plugin that recorded an instrument the library would not take would say
        // that a program is in use when it is not.
        const std::vector<char> bytes = input.bytes(sizeof(Native_Instrument));
        Native_Instrument native {};
        std::memcpy(&native, bytes.data(), bytes.size());
        return Instrument::from_adlmidi(native);
    }
    default:
        return held;
    }
}

// The bookkeeping of the banks, on its own: what the plugin holds has to hold
// together whether or not anybody is told about it.
void check_slots(const AdlplugAudioProcessor &processor, unsigned &lookups_left)
{
    Bank_Manager &bm = processor.bank_manager();
    const std::array<Bank_Manager::Bank_Info, bank_reserve_size> &infos = bm.bank_infos();

    for (unsigned i = 0; i < bank_reserve_size; ++i) {
        const Bank_Manager::Bank_Info &info = infos[i];

        // The count of a slot's programs is the number of its bits.
        std::size_t bits = 0;
        for (unsigned p = 0; p < Bank_Manager::program_count; ++p)
            bits += info.used.test(p) ? 1u : 0u;
        FUZZ_CHECK(info.used.count() == bits);

        // A slot that is not a bank holds nothing: the editor's list of banks,
        // the state a project keeps and the search for a free slot all skip
        // such a slot, so a program in one could neither be seen, be saved,
        // nor stay where it is.
        if (!info) {
            FUZZ_CHECK(info.used.none());
            continue;
        }

        // Two slots of the same bank would leave the second unreachable: every
        // search stops at the first.
        for (unsigned j = 0; j < i; ++j)
            FUZZ_CHECK(!(static_cast<bool>(infos[j]) && infos[j].id == info.id));

        // A program is in use exactly when the instrument in it is not blank.
        if (lookups_left < Bank_Manager::program_count)
            continue;
        lookups_left -= Bank_Manager::program_count;
        Instrument ins;
        for (unsigned p = 0; p < Bank_Manager::program_count; ++p) {
            FUZZ_CHECK(bm.find_program(info.id, p, ins));
            FUZZ_CHECK(info.used.test(p) == !ins.blank());
        }
    }
}

// What the plugin tells the editor has to be what the plugin holds. The
// notifications go out at the end of the block that handles the messages, so
// this reads them against the state the same block left behind.
void check_notification(const AdlplugAudioProcessor &processor, const Buffered_Message &msg)
{
    Bank_Manager &bm = processor.bank_manager();
    const std::array<Bank_Manager::Bank_Info, bank_reserve_size> &infos = bm.bank_infos();

    switch (msg.header.tag) {
    case std::to_underlying(Fx_Message::NotifyBankSlots): {
        const auto body = Messages::body<Messages::Fx::NotifyBankSlots>(msg);
        FUZZ_CHECK(body.count <= bank_reserve_size);
        unsigned n = 0;
        for (const Bank_Manager::Bank_Info &info : infos) {
            if (!info || info.used.none())
                continue;
            FUZZ_CHECK(n < body.count);
            const Messages::Fx::NotifyBankSlots::Entry &entry = body.entry[n++];
            FUZZ_CHECK(entry.bank == info.id);
            FUZZ_CHECK(entry.used == info.used);
            static_assert(sizeof entry.name == sizeof info.bank_name);
            FUZZ_CHECK(std::memcmp(entry.name, info.bank_name, sizeof entry.name) == 0);
        }
        FUZZ_CHECK(n == body.count);
        break;
    }
    case std::to_underlying(Fx_Message::NotifyInstrument): {
        const auto body = Messages::body<Messages::Fx::NotifyInstrument>(msg);
        FUZZ_CHECK(body.program < Bank_Manager::program_count);
        Instrument held;
        FUZZ_CHECK(bm.find_program(body.bank, body.program, held));
        FUZZ_CHECK(body.instrument.equal_instrument(held));

        // The name is the bank manager's, not the library's.
        bool found = false;
        for (const Bank_Manager::Bank_Info &info : infos) {
            if (!info || !(info.id == body.bank))
                continue;
            const std::span<const char, Bank_Manager::name_size> name = info.program_name(body.program);
            static_assert(sizeof body.instrument.name == Bank_Manager::name_size);
            FUZZ_CHECK(std::memcmp(body.instrument.name, name.data(), name.size()) == 0);
            found = true;
            break;
        }
        FUZZ_CHECK(found);
        break;
    }
    default:
        // The other notifications are not about the banks.
        break;
    }
}

// One block of audio, which is where everything happens; the notifications it
// sends are read back and, if the state has settled, checked.
void play_block(AdlplugAudioProcessor &processor, unsigned frames, bool check)
{
    std::array<float, block_size> left {};
    std::array<float, block_size> right {};
    // The samples are written through this, by the processor the buffer is handed
    // to. The check looks for a write through the array itself, finds none, and
    // offers a pointee that is const -- which the buffer could not be given
    // either, since it refers to the samples to write them.
    // NOLINTNEXTLINE(misc-const-correctness)
    float *channels[2] {left.data(), right.data()};
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer(channels, 2, static_cast<int>(frames));
    processor.processBlock(buffer, midi);

    for (unsigned i = 0; i < frames; ++i)
        FUZZ_CHECK(std::isfinite(left[i]) && std::isfinite(right[i]));

    Simple_Fifo &queue = processor.message_queue_to_ui_rt();
    while (const Buffered_Message msg = Messages::read(queue)) {
        if (check)
            check_notification(processor, msg);
        Messages::finish_read(queue, msg);
    }
}

// A message to the processor, as the editor writes one; false when the queue had
// no room for it, which is what the editor answers by keeping it for later.
template <class T, class Fill>
bool send(AdlplugAudioProcessor &processor, Fill &&fill)
{
    const std::shared_ptr<Simple_Fifo> queue = processor.message_queue_for_ui();
    return queue && Messages::send<T>(*queue, std::forward<Fill>(fill));
}

// A name into a field of a message: a field is of a fixed size and carries a
// terminator only when the name is shorter, which is how a bank file holds one.
void set_name(std::span<char> field, const std::vector<char> &name)
{
    std::fill(std::copy_n(name.begin(), std::min(name.size(), field.size()), field.begin()),
              field.end(), '\0');
}

// The words that a file of the library's formats begins with. An input that
// begins with one of them is that file, whole, so that a bank of the submodules
// is a seed as it is: nothing a fuzzer makes up comes upon the words by itself.
#if defined(ADLPLUG_OPL3)
constexpr std::string_view bank_file_mark = "WOPL3-BANK";
constexpr std::string_view instrument_file_mark = "WOPL3-INST";
#elif defined(ADLPLUG_OPN2)
constexpr std::string_view bank_file_mark = "WOPN2-B";
constexpr std::string_view instrument_file_mark = "WOPN2-INST";
#endif

bool begins_with(std::span<const std::uint8_t> bytes, std::string_view mark) noexcept
{
    return bytes.size() >= mark.size() && std::memcmp(bytes.data(), mark.data(), mark.size()) == 0;
}

// Which file an input begins as, if it begins as one at all. The formats are the
// ones the editor opens: a bank file, an instrument file of the library's format,
// and, where the chip has it, the other format of one instrument, whose files are
// among the seeds as well (fuzz/dict/wopl.dict has every word that begins one).
enum class File_Kind { bank, instrument, other_instrument };

std::optional<File_Kind> file_kind(std::span<const std::uint8_t> bytes) noexcept
{
    if (begins_with(bytes, instrument_file_mark))
        return File_Kind::instrument;
    if (begins_with(bytes, bank_file_mark))
        return File_Kind::bank;
#if defined(ADLPLUG_OPL3)
    if (begins_with(bytes, "SBI") || begins_with(bytes, "2OP") ||
        begins_with(bytes, "4OP"))
        return File_Kind::other_instrument;
#endif
    return {};
}

// The messages of a file go into the editor's queue, because that is whose they
// are: sources/bank_load.h reads the bytes and says what to send, and what sends
// it is the caller's -- the editor keeps what the queue cannot take and sends it
// again, and this writes straight to the queue.
auto sender(AdlplugAudioProcessor &processor)
{
    return [&processor](const auto &message) {
        using Message = std::decay_t<decltype(message)>;
        // A file is more messages than the queue holds at once: the editor keeps
        // what it cannot send and sends it again once the audio thread has made
        // room, and the block that makes the room is played here on the spot, so
        // that a whole file arrives rather than the first bank of it. A message
        // still dropped after that is a host that never came back for the audio,
        // and what the plugin holds has to add up either way.
        for (unsigned tries = 0; tries < send_tries; ++tries) {
            if (send<Message>(processor, [&message](Message &body) { body = message; }))
                return;
            play_block(processor, block_size, false);
        }
    };
}

// A file, read and sent the way the editor reads and sends one. This is the join
// that the other targets leave open: fuzz/bank_file.cc gives the library's reader
// any bytes and looks at what comes back, and the records here give the messages
// any values, while what is between the two -- every bank and every program of a
// file turned into messages, and the bank manager left holding them -- used to be
// inside the editor where nothing could reach it.
//
// The title is longer than the field it goes into, since a file's name is not the
// file's to choose and the editor cuts it to fit.
void load_bank_file(AdlplugAudioProcessor &processor, std::span<const std::uint8_t> bytes,
                    unsigned part)
{
    // The bytes of an input belong to the fuzzer, and the library's reader takes
    // memory it may write to (sources/bank_load.h): it gets a copy.
    std::vector<std::uint8_t> own(bytes.begin(), bytes.end());
    if (const std::optional<Bank_File_Contents> contents = read_bank_file(own))
        send_bank_file(sender(processor), *contents,
                       "a bank file whose title is longer than the field it goes into", part);
}

void load_instrument_file(AdlplugAudioProcessor &processor, std::span<const std::uint8_t> bytes,
                          int format, Bank_Id bank, std::uint8_t program, unsigned part)
{
    std::vector<std::uint8_t> own(bytes.begin(), bytes.end());
    if (const std::optional<Instrument> instrument = read_instrument_file(own, format))
        send_instrument_file(sender(processor), *instrument, bank, program, part);
}

// How many bytes of a name a record asks for. The whole field is among them,
// where the name has no terminator and its last character may be cut in half.
std::size_t name_length(unsigned choice, std::size_t field_size)
{
    switch (choice & 3u) {
    case 0:  return 1;
    case 1:  return field_size / 4;
    case 2:  return field_size * 3 / 4;
    default: return field_size;
    }
}

}  // namespace

// The processor's interface is not built here, so this target says what the
// processor has instead of an editor: nothing (plugin_editor.cc holds the other
// two lines of this pair).
bool AdlplugAudioProcessor::hasEditor() const
{
    return false;
}

juce::AudioProcessorEditor *AdlplugAudioProcessor::createEditor()
{
    return nullptr;
}

int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    // The parameters of a processor want JUCE alive. libFuzzer calls this in one
    // thread, so one initialiser for the whole run will do.
    static const juce::ScopedJuceInitialiser_GUI juce_alive;

    Input input(data, size);
    AdlplugAudioProcessor processor;
    processor.prepareToPlay(rate, block_size);

    // A file the user opens, whole: an input that begins as one of the files the
    // editor opens is that file and nothing else, read and sent as the editor does
    // it. The blocks at the end then let the plugin take the messages, and the
    // checks look at what it made of them.
    const std::span<const std::uint8_t> whole(data, size);
    const std::optional<File_Kind> kind = file_kind(whole);
    if (kind) {
        switch (*kind) {
        case File_Kind::bank:
            load_bank_file(processor, whole, 0);
            break;
        case File_Kind::instrument:
            load_instrument_file(processor, whole, 0, Bank_Id(0, 0, false), 0, 0);
            break;
        case File_Kind::other_instrument:
            load_instrument_file(processor, whole, 1, Bank_Id(0, 0, false), 0, 0);
            break;
        }
    }

    unsigned records_left = records_max;
    unsigned prepares_left = prepares_max;
    unsigned lookups_left = lookups_max;

    while (!kind && !input.done() && records_left-- > 0) {
        const std::uint8_t record = input.byte();
        const unsigned value = record & 15u;
        const bool notify = (value & 1u) != 0;
        const bool percussive = (value & 2u) != 0;

        switch (record >> 4) {
        case 0:
            play_block(processor, std::min(1u + value * 4u, unsigned{block_size}), false);
            check_slots(processor, lookups_left);
            break;
        case 1:
            switch (value & 7u) {
            case 1:
                send<Messages::User::RequestFullBankState>(processor, [](auto &) {});
                break;
            case 2:
                send<Messages::User::RequestChipSettings>(processor, [](auto &) {});
                break;
            case 3: {
                const unsigned mask = input.two_bytes();
                send<Messages::User::RequestSelections>(processor,
                    [mask](auto &body) { body.channel_mask = std::bitset<16>(mask); });
                break;
            }
            case 4:
                send<Messages::User::RequestActivePart>(processor, [](auto &) {});
                break;
            case 5:
                send<Messages::User::RequestBankTitle>(processor, [](auto &) {});
                break;
            default:
                send<Messages::User::RequestBankSlots>(processor, [](auto &) {});
                break;
            }
            break;
        case 2: {
            // Loading an instrument is what a bank file, a project and every knob
            // of the editor end at. The byte after the program says what else the
            // message carries: the part it is for, which is a number of its own
            // and need not be one of the sixteen, and in its lowest two bits
            // whether the plugin tells the editor and whether it has the worker
            // measure what it was given.
            const Bank_Id bank = bank_from(input, percussive);
            const std::uint8_t program = input.byte();
            const std::uint8_t how = input.byte();
            Instrument held;
            processor.bank_manager().find_program(bank, program, held);
            const Instrument ins = instrument_from(input, value >> 2, held);
            send<Messages::User::LoadInstrument>(processor, [&](auto &body) {
                body.part = how;
                body.bank = bank;
                body.program = program;
                body.instrument = ins;
                // A measurement has the worker play the instrument for as long as
                // forty seconds and listen for sixty more, which is what
                // fuzz/measurement.cc is about; asked for here, an input pays for
                // the one that is running when it ends.
                body.need_measurement = (how & 2u) != 0;
                body.notify_back = (how & 1u) != 0;
            });
            break;
        }
        case 3: {
            const Bank_Id bank = bank_from(input, percussive);
            const std::uint8_t program = input.byte();
            send<Messages::User::CreateInstrument>(processor, [&](auto &body) {
                body.bank = bank;
                body.program = program;
                body.notify_back = notify;
            });
            break;
        }
        case 4: {
            const Bank_Id bank = bank_from(input, percussive);
            const std::uint8_t program = input.byte();
            send<Messages::User::DeleteInstrument>(processor, [&](auto &body) {
                body.bank = bank;
                body.program = program;
                body.notify_back = notify;
            });
            break;
        }
        case 5: {
            const Bank_Id bank = bank_from(input, percussive);
            send<Messages::User::DeleteBank>(processor, [&](auto &body) {
                body.bank = bank;
                body.notify_back = notify;
            });
            break;
        }
        case 6: {
            const Bank_Id bank = bank_from(input, percussive);
            const std::vector<char> name =
                input.bytes(name_length(value >> 2, Bank_Manager::name_size));
            send<Messages::User::RenameBank>(processor, [&](auto &body) {
                body.bank = bank;
                body.notify_back = notify;
                set_name(body.name, name);
            });
            break;
        }
        case 7: {
            const Bank_Id bank = bank_from(input, percussive);
            const std::uint8_t program = input.byte();
            const std::vector<char> name =
                input.bytes(name_length(value >> 2, Bank_Manager::name_size));
            send<Messages::User::RenameProgram>(processor, [&](auto &body) {
                body.bank = bank;
                body.program = program;
                body.notify_back = notify;
                set_name(body.name, name);
            });
            break;
        }
        case 8: {
            // Selecting a program puts it in the parameters, which a block then
            // loads back into the bank: the editor's other way in.
            const Bank_Id bank = bank_from(input, percussive);
            const std::uint8_t program = input.byte();
            send<Messages::User::SelectProgram>(processor, [&](auto &body) {
                body.part = value;
                body.bank = bank;
                body.program = program;
            });
            break;
        }
        case 9: {
            // The active part is a number of its own as well.
            const std::uint8_t part = input.byte();
            send<Messages::User::SetActivePart>(processor,
                [part](auto &body) { body.part = part; });
            break;
        }
        case 10: {
            const std::vector<char> title =
                input.bytes(name_length(value >> 2, sizeof(Messages::User::SetBankTitle::title)));
            send<Messages::User::SetBankTitle>(processor,
                [&title](auto &body) { set_name(body.title, title); });
            break;
        }
        case 11:
            send<Messages::User::ClearBanks>(processor,
                [notify](auto &body) { body.notify_back = notify; });
            break;
        case 12: {
            // The worker's answer, as it comes back: the delays go in only if
            // the instrument is still the one that was measured.
            const Bank_Id bank = bank_from(input, percussive);
            const std::uint8_t program = input.byte();
            const auto kon = static_cast<std::uint16_t>(input.two_bytes());
            const auto koff = static_cast<std::uint16_t>(input.two_bytes());
            Instrument held;
            processor.bank_manager().find_program(bank, program, held);
            const Instrument ins = instrument_from(input, value >> 2, held);
            send<Messages::Worker::MeasurementResult>(processor, [&](auto &body) {
                body.bank = bank;
                body.program = program;
                body.instrument = ins;
                body.ms_sound_kon = kon;
                body.ms_sound_koff = koff;
            });
            break;
        }
        case 13: {
            const std::uint8_t model = input.byte();
            const std::uint8_t how = input.byte();
            send<Messages::User::LoadGlobalParameters>(processor, [&](auto &body) {
                // The volume model is what both chips have; what else is global
                // differs between them, and the byte after it says.
                body.param.volume_model = model;
#if defined(ADLPLUG_OPL3)
                body.param.deep_tremolo = (how & 1u) != 0;
                body.param.deep_vibrato = (how & 2u) != 0;
                body.param.mt32_defaults = (how & 4u) != 0;
#elif defined(ADLPLUG_OPN2)
                body.param.lfo_enable = (how & 1u) != 0;
                body.param.lfo_frequency = how >> 1;
#endif
                body.notify_back = notify;
            });
            break;
        }
        case 14:
            // A host stops and starts the plugin again. The plugin carries its
            // state over by writing it and reading it back, so what it would
            // save cannot change by being carried.
            if (prepares_left > 0) {
                --prepares_left;
                juce::MemoryBlock before;
                processor.getStateInformation(before);
                processor.releaseResources();
                processor.prepareToPlay(rate, block_size);
                juce::MemoryBlock after;
                processor.getStateInformation(after);
                FUZZ_CHECK(before == after);
            }
            break;
        default:
            // Two things, by the low bit: the best number of four-operator
            // channels, or the rest of the input as a file, which is how a
            // sequence of messages can have a file opened in the middle of it.
            if ((value & 1u) == 0) {
#if defined(ADLPLUG_OPL3)
                send<Messages::User::SelectOptimal4Ops>(processor, [](auto &) {});
#endif
                break;
            }
            if ((value & 4u) != 0) {
                const Bank_Id bank = bank_from(input, percussive);
                const std::uint8_t program = input.byte();
                load_instrument_file(processor, input.rest(), ((value & 8u) != 0) ? 1 : 0,
                                     bank, program, 0);
            }
            else {
                load_bank_file(processor, input.rest(), 0);
            }
            break;
        }
    }

    // What the editor is shown at the end: let the parameters settle what they
    // owe, ask for everything, and read the answers against what is held.
    for (unsigned i = 0; i < settle_blocks; ++i)
        play_block(processor, block_size, false);
    send<Messages::User::RequestBankSlots>(processor, [](auto &) {});
    send<Messages::User::RequestFullBankState>(processor, [](auto &) {});
    for (unsigned i = 0; i < notify_blocks; ++i)
        play_block(processor, block_size, true);

    lookups_left = bank_reserve_size * Bank_Manager::program_count;
    check_slots(processor, lookups_left);

    processor.releaseResources();
    return 0;
}
