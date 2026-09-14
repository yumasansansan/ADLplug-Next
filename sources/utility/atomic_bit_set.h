// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// A fixed-size bitset whose individual bits can be set and cleared from
// different threads without a lock. This replaces folly::AtomicBitSet, which
// upstream removed; the interface follows folly's (Apache-2.0) design so that
// call sites did not have to change.
//
// Note that N counts *bits*, not blocks. folly's AtomicBitSet<N> allocated N
// blocks and asserted `idx < N * kBitsPerBlock`, which over-allocated by a
// factor of 32; upstream fixed that when the class became ConcurrentBitSet,
// and the corrected sizing is what is implemented here.

#pragma once
#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <limits>

template <std::size_t N>
class Atomic_Bit_Set {
public:
    Atomic_Bit_Set() = default;

    Atomic_Bit_Set(const Atomic_Bit_Set &) = delete;
    Atomic_Bit_Set &operator=(const Atomic_Bit_Set &) = delete;

    // Set bit `idx`, returning its previous value.
    bool set(std::size_t idx, std::memory_order order = std::memory_order_seq_cst);

    // Clear bit `idx`, returning its previous value.
    bool reset(std::size_t idx, std::memory_order order = std::memory_order_seq_cst);

    // Assign bit `idx`, returning its previous value.
    bool set(std::size_t idx, bool value, std::memory_order order = std::memory_order_seq_cst);

    // Clear every bit. Not atomic as a whole: a concurrent set() of another
    // bit may or may not survive.
    void reset_all(std::memory_order order = std::memory_order_seq_cst);

    bool test(std::size_t idx, std::memory_order order = std::memory_order_seq_cst) const;
    bool operator[](std::size_t idx) const { return test(idx); }

    constexpr std::size_t size() const { return N; }

private:
    using Block = unsigned int;
    using Atomic_Block = std::atomic<Block>;

    // The audio thread touches this, so a lock-free representation is not
    // merely preferable here; a mutex-backed std::atomic would be a bug.
    static_assert(Atomic_Block::is_always_lock_free,
                  "Atomic_Bit_Set requires a lock-free atomic block type");

    static constexpr std::size_t bits_per_block_ = std::numeric_limits<Block>::digits;
    static constexpr std::size_t block_count_ = (N + bits_per_block_ - 1) / bits_per_block_;

    static constexpr std::size_t block_index(std::size_t bit) { return bit / bits_per_block_; }
    static constexpr Block bit_mask(std::size_t bit) { return Block{1} << (bit % bits_per_block_); }

    std::array<Atomic_Block, block_count_> data_ {};
};

template <std::size_t N>
inline bool Atomic_Bit_Set<N>::set(std::size_t idx, std::memory_order order)
{
    assert(idx < N);
    const Block mask = bit_mask(idx);
    return (data_[block_index(idx)].fetch_or(mask, order) & mask) != 0;
}

template <std::size_t N>
inline bool Atomic_Bit_Set<N>::reset(std::size_t idx, std::memory_order order)
{
    assert(idx < N);
    const Block mask = bit_mask(idx);
    return (data_[block_index(idx)].fetch_and(~mask, order) & mask) != 0;
}

template <std::size_t N>
inline bool Atomic_Bit_Set<N>::set(std::size_t idx, bool value, std::memory_order order)
{
    return value ? set(idx, order) : reset(idx, order);
}

template <std::size_t N>
inline void Atomic_Bit_Set<N>::reset_all(std::memory_order order)
{
    for (Atomic_Block &block : data_)
        block.store(0, order);
}

template <std::size_t N>
inline bool Atomic_Bit_Set<N>::test(std::size_t idx, std::memory_order order) const
{
    assert(idx < N);
    const Block mask = bit_mask(idx);
    return (data_[block_index(idx)].load(order) & mask) != 0;
}
