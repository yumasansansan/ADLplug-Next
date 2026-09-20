// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

#include "test.h"
#include "utility/atomic_bit_set.h"
#include "utility/counting_bitset.h"
#include "utility/field_bitops.h"
#include "utility/fourcc.h"
#include "utility/semaphore.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <thread>
#include <vector>

namespace {

// Writes every value into a field of a byte, over a few backgrounds, and reads
// it back plainly and inverted.
template <unsigned shift, unsigned size>
void check_byte_field()
{
    const unsigned max = (1u << size) - 1;
    const unsigned others = ~(max << shift) & 0xffu;
    for (const unsigned background : {0x00u, 0xffu, 0xa5u}) {
        for (unsigned value = 0; value <= max; ++value) {
            auto byte = static_cast<std::uint8_t>(background);
            Field_Bitops::set<shift, size>(byte, value);
            CHECK(Field_Bitops::get<shift, size, unsigned>(byte) == value);
            CHECK((static_cast<unsigned>(byte) & others) == (background & others));
            Field_Bitops::set_inverted<shift, size>(byte, value);
            CHECK(Field_Bitops::get_inverted<shift, size, unsigned>(byte) == value);
            CHECK(Field_Bitops::get<shift, size, unsigned>(byte) == max - value);
        }
    }
}

}  // namespace

ADLPLUG_TEST(counting_bitset)
{
    counting_bitset<128> bits;
    CHECK(bits.none() && bits.count() == 0);
    bits.set(0).set(64).set(127);
    CHECK(bits.count() == 3 && bits.test(64) && !bits.test(63));
    bits.set(64);
    CHECK(bits.count() == 3);
    bits.reset(0);
    CHECK(bits.count() == 2 && !bits.test(0));
    bits.flip();
    CHECK(bits.count() == 126 && bits.test(0) && !bits.test(127));
    bits.flip(127);
    CHECK(bits.count() == 127);
    bits.set();
    CHECK(bits.all() && bits.count() == 128);
    bits.reset();
    CHECK(bits.none());
}

ADLPLUG_TEST(atomic_bit_set)
{
    Atomic_Bit_Set<100> bits;
    CHECK(bits.size() == 100);
    // The blocks are 32 bits wide: bits on both sides of each boundary.
    for (const std::size_t i : {0uz, 31uz, 32uz, 63uz, 64uz, 99uz}) {
        CHECK(!bits.test(i));
        CHECK(!bits.set(i));
        CHECK(bits.set(i));
        CHECK(bits.test(i));
    }
    CHECK(!bits.test(1) && !bits.test(33) && !bits.test(65));
    CHECK(bits.reset(31));
    CHECK(!bits.reset(31));
    CHECK(!bits.set(31, false));
    CHECK(!bits.set(30, true));
    CHECK(bits[30]);
    bits.reset_all();
    bool none = true;
    for (std::size_t i = 0; i < bits.size(); ++i)
        none = none && !bits.test(i);
    CHECK(none);
}

ADLPLUG_TEST(atomic_bit_set_threads)
{
    // Four threads set the bits of their own residue at once; every one sticks.
    Atomic_Bit_Set<1024> bits;
    std::vector<std::thread> threads;
    threads.reserve(4);
    for (std::size_t t = 0; t < 4; ++t) {
        threads.emplace_back([&bits, t] {
            for (std::size_t i = t; i < bits.size(); i += 4)
                bits.set(i);
        });
    }
    for (std::thread &thread : threads)
        thread.join();
    bool all = true;
    for (std::size_t i = 0; i < bits.size(); ++i)
        all = all && bits.test(i);
    CHECK(all);
}

ADLPLUG_TEST(field_bitops)
{
    check_byte_field<0, 1>();
    check_byte_field<7, 1>();
    check_byte_field<0, 4>();
    check_byte_field<4, 4>();
    check_byte_field<2, 3>();
    check_byte_field<5, 3>();
    check_byte_field<0, 8>();
}

ADLPLUG_TEST(fourcc)
{
    CHECK(fourcc("chip") == 0x63686970u);
    CHECK(fourcc("\xff\x01\x00\x80") == 0xff010080u);
}

ADLPLUG_TEST(semaphore)
{
    Semaphore empty;
    CHECK(!empty.try_wait());

    Semaphore two(2);
    CHECK(two.try_wait());
    CHECK(two.try_wait());
    CHECK(!two.try_wait());
    two.post();
    CHECK(two.try_wait());

    // wait() returns once another thread has posted.
    Semaphore woken;
    std::atomic<bool> posted {false};
    std::thread poster([&woken, &posted] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        posted.store(true);
        woken.post();
    });
    woken.wait();
    CHECK(posted.load());
    poster.join();
}
