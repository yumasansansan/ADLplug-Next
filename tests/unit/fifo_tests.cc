//     Part of ADLplug, distributed under the GNU GPL v3 or later.
//               (See accompanying file LICENSE.)

#include "test.h"
#include "utility/simple_fifo.h"
#include <cstdint>

ADLPLUG_TEST(simple_fifo_round_trip)
{
    // Messages of changing lengths go around the ring buffer many times. Each
    // read has to see its message in one piece, also when it was written
    // across the end of the buffer.
    const unsigned alignment = Simple_Fifo::alignment;
    Simple_Fifo fifo(8 * alignment);
    std::uint8_t next = 0;
    for (unsigned round = 0; round < 64; ++round) {
        const unsigned length = 1 + round % (3 * alignment);

        unsigned write_offset = 0;
        std::uint8_t *const written = fifo.write(length, write_offset);
        CHECK(written != nullptr);
        if (written == nullptr)
            return;
        for (unsigned i = 0; i < length; ++i)
            written[i] = static_cast<std::uint8_t>(next + i);
        CHECK(fifo.write_padding(write_offset));
        CHECK(write_offset % alignment == 0);
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
        CHECK(fifo.read_padding(read_offset));
        CHECK(read_offset == write_offset);
        fifo.finish_read(read_offset);
        CHECK(fifo.get_num_ready() == 0);

        next = static_cast<std::uint8_t>(next + length);
    }
}

ADLPLUG_TEST(simple_fifo_full)
{
    // AbstractFifo keeps one byte of its capacity free, so a FIFO of four
    // alignments takes three aligned messages, and refuses a fourth.
    const unsigned alignment = Simple_Fifo::alignment;
    Simple_Fifo fifo(4 * alignment);
    unsigned messages = 0;
    for (;;) {
        unsigned offset = 0;
        if (fifo.write(alignment, offset) == nullptr || !fifo.write_padding(offset))
            break;
        fifo.finish_write(offset);
        ++messages;
    }
    CHECK(messages == 3);
    CHECK(fifo.get_num_ready() == 3 * alignment);
}
