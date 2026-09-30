// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// ADLplug-Next's emulator core of the YMF262 (OPL3). It makes what the
// low-level core drawn from a die shot of the chip (YMF262-LLE) makes as
// libADLMIDI drives it, sample for sample, and makes it fast: the 36 slots
// are worked out together, in loops the compiler vectorizes.
//
// libADLMIDI feeds that core a register write every 2 2/9 samples, and each
// write takes effect in the middle of a pass over the slots, for the slots that
// read their registers after it; the core here keeps the same queue, and
// follows the chip in when each slot reads each register. What a slot works
// out in a pass follows the code of YMF262-LLE and Nuked OPL3 (ymf262_logic.h).

#pragma once

#include "ymf262_logic.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace adlplug::opl3 {

class Core {
public:
    Core() { reset(); }

    // The chip as libADLMIDI's wrapper of the low-level core leaves it when it
    // resets it.
    void reset();
    // Queues a register write. The writes take effect one after another, a
    // write every 2 2/9 samples, each at the slot time it reaches the chip.
    void write(std::uint16_t address, std::uint8_t value);
    // Soft panning of a channel (the address of its 0xc0 register): 0 left
    // only, 64 both at full, 127 right only.
    void write_pan(std::uint16_t address, std::uint8_t value);
    // One sample at the chip's rate: frame[0] is output A, frame[1] output B.
    void generate(std::int32_t *frame);

    static constexpr int slots = 36;
    static constexpr int channels = 18;

private:
    // The slots are kept in lanes, in the order of the stages of modulation:
    // a slot in a stage takes its modulation, when it has one from another
    // slot, from the slot at the same position of the stage before. The
    // stages of 12, 12, 6 and 6 slots take 16, 16, 8 and 8 lanes, the lanes
    // past a stage's slots never heard, so that every loop over the lanes runs
    // in whole vectors of 8 or 16 lanes of 32 bits, and a stage reads the lanes
    // of the stage before but never its own.
    static constexpr int stage_begin[4] = {0, 16, 32, 40};
    static constexpr int stage_width[4] = {16, 16, 8, 8};
    static constexpr int lanes = 48;
    static constexpr int queue_size = 2048;

    struct Landing {
        bool due = false;
        std::uint16_t address = 0;
        std::uint8_t value = 0;
        int slot_time = 0;  // the slot time of its pass it takes effect at
    };

    void schedule_writes(Landing &now, Landing &next);
    void land(const Landing &l);
    void apply(std::uint16_t address, std::uint8_t value);
    void apply_array(int bank, int reg, std::uint8_t value);
    void spread(std::size_t channel);
    void relatch();
    void refresh_routing();
    void derive();
    void derive_soft_pan();
    // The parts of a pass, each called once from generate(), and always
    // inlined into it.
    [[gnu::always_inline]] void envelope_and_phase();
    [[gnu::always_inline]] void rhythm_phases();
    template <int Begin, int Before, int Width>
    [[gnu::always_inline]] void stage();
    [[gnu::always_inline]] void operators();
    [[gnu::always_inline]] void store_history();
    [[gnu::always_inline]] void accumulate(std::int32_t *frame);

    // By lane: what the passes work with, worked out from what the slots read
    // (below) when that changes (ymf262_logic.h says what each is). These,
    // and the state after them, come first: they are what every pass reads,
    // aligned for the vectors, and nothing smaller pads them apart.
    alignas(64) std::array<std::uint32_t, lanes> rates_{};
    alignas(64) std::array<std::int32_t, lanes> key_scale_{}, key_scale_low_{}, sustain_{}, key_on_{}, level_offset_{};
    alignas(64) std::array<std::int32_t, lanes> tremolo_depth_{};  // 0 none, 1 shallow, 2 deep
    alignas(64) std::array<std::uint32_t, lanes> fnum_v_{}, block_v_{}, multiple_v_{}, vibrato_{}, vibrato_depth_{};
    alignas(64) std::array<std::int32_t, lanes> phase_shift_{}, turn_bit_{}, mute_bit_{}, sign_bit_{}, square_{}, sawtooth_{};
    alignas(64) std::array<std::int32_t, lanes> modulated_by_{}, feedback_shift_{};  // 0 nothing, 1 the stage before, 2 feedback
    alignas(64) std::array<std::int32_t, lanes> a_now_{}, a_late_{}, b_now_{}, b_late_{};  // how loud on each output
    // and with soft panning, in sixteenths of sixteen bits
    alignas(64) std::array<std::int32_t, lanes> soft_a_now_{}, soft_a_late_{}, soft_b_now_{}, soft_b_late_{};
    // Soft panning: the gains of outputs A and B of the slot's channel, 65536
    // for full.
    alignas(64) std::array<std::int32_t, lanes> gain_a_{}, gain_b_{};
    // The slot (-1 for a lane that has none), whether it is its channel's
    // first, and whether it is in the second channel of a chain of four: what
    // the routing and derive() compare with constants. They are constants,
    // kept in the core all the same (reset() fills them): from a constant table
    // the compiler works each such comparison out from the lane's number
    // instead, in 64 bits, as 48 lanes do not fit in 32, and the vectorized
    // loops pay for that.
    alignas(64) std::array<std::int32_t, lanes> lane_slot_{}, lane_first_{}, lane_second_channel_{};

    // By lane: the state.
    alignas(64) std::array<std::uint32_t, lanes> phase_{};
    alignas(64) std::array<std::int32_t, lanes> level_{}, state_{};
    alignas(64) std::array<std::int32_t, lanes> out_{}, out_1_{}, out_2_{};  // this pass's, the last two
    alignas(64) std::array<std::int32_t, lanes> eg_out_{}, phase_out_{};
    alignas(64) std::array<std::int32_t, lanes> restart_{};  // keyed on from release in this pass

    std::int64_t a_carry_soft_ = 0, b_carry_soft_ = 0;  // what the slots heard late made in the last pass, soft panned
    ymf262::Counters counters_;
    std::int32_t a_carry_ = 0, b_carry_ = 0;  // and without soft panning
    std::uint32_t noise_ = 0;  // as the pass's first step leaves it
    ymf262::Rhythm_bits rhythm_bits_;
    // The chip stores the last two values of slots m and m + 18 for feedback
    // in one queue of four, which moves on as either value is stored. Rhythm
    // mode stops storing those of slots 13 and 14, and so mixes theirs with
    // those of slots 31 and 32; the two queues are kept as they are.
    std::array<std::array<std::int32_t, 4>, 2> shared_history_{};

    // The queue of writes, and the slot times until it may act again.
    int queue_head_ = 0, queue_count_ = 0, queue_wait_ = 0;
    // The last write to take effect, while some slots still hold what they
    // read before it: the passes since, and its slot time.
    int landed_age_ = -1, landed_slot_time_ = 0;
    // The last write to the registers from 0x20 on: its bank as it was
    // decoded, its register and its data (array_value_ below).
    int array_bank_ = 0, array_reg_ = 0;
    Landing landing_next_;  // a write that takes effect in the next pass
    struct Pending {
        std::uint16_t address;
        std::uint8_t value;
        bool address_written;
    };
    std::array<Pending, queue_size> queue_{};

    // Registers as written: the slots' by lane, the channels' by channel.
    std::array<std::uint16_t, channels> fnum_{};
    std::array<std::uint8_t, lanes> r20_{}, r40_{}, r60_{}, r80_{}, re0_{};
    std::array<std::uint8_t, channels> block_{}, key_{}, con_{}, fb_{}, pan_{};
    std::array<std::uint8_t, channels> con_pair_{};  // the connection as a chain of four reads it
    std::uint8_t newm_ = 0, four_op_ = 0, nts_ = 0, rhythm_ = 0, drum_keys_ = 0, dam_ = 0, dvb_ = 0;
    std::uint8_t array_value_ = 0;
    bool soft_pan_ = false;

    // By lane: the channels' registers as they stand, for the slots that read
    // them (spread()), so that no loop over the lanes reads another lane's:
    // those of the slot's own channel, but in the second channel of a chain of
    // four the first's frequency, block and key; and in a chain, the first
    // channel's connection and the second's as a chain reads it.
    std::array<std::uint16_t, lanes> ch_fnum_{};
    std::array<std::uint8_t, lanes> ch_block_{}, ch_key_{}, ch_con_{}, ch_fb_{}, ch_pan_{};
    std::array<std::uint8_t, lanes> first_con_{}, second_con_{};

    // By lane: the registers as the slot last read them. The chip reads a
    // slot's own registers a slot time before its turn, the feedback of its
    // channel four before, and the registers of the whole chip at its turn.
    std::array<std::uint16_t, lanes> lfnum_{};
    std::array<std::uint8_t, lanes> l20_{}, l40_{}, l60_{}, l80_{}, le0_{};
    std::array<std::uint8_t, lanes> lblock_{}, lkey_{}, lpan_{}, lfb_{};
    // The channel's connection as the channel's first slot read it, and, in a
    // chain of four, the first channel's connection and the second's as a
    // chain reads it, as the chain's first slot read them.
    std::array<std::uint8_t, lanes> lcon_{}, lfirst_con_{}, lsecond_con_{};
    std::array<std::uint8_t, lanes> lnts_{}, ldrum_keys_{}, ldam_{}, ldvb_{}, lfour_op_{};
    // The rhythm flag, as read for the drums' keys, for stopping feedback and
    // modulation, for the drums' phases, and for what is heard.
    std::array<std::uint8_t, lanes> lrhythm_keys_{}, lrhythm_before_{}, lrhythm_{}, lrhythm_after_{};

    // By lane: what the routing makes of them.
    std::array<std::uint8_t, lanes> modulation_{};    // 0 none, 1 the stage before, 2 feedback
    std::array<std::uint8_t, lanes> weight_{};        // 0 not heard, 1, or 2 for a drum
    std::array<std::uint8_t, lanes> rhythm_phase_{};  // 0, or 1 hi-hat, 2 snare, 3 top cymbal
    std::array<std::uint8_t, lanes> hold_history_{};  // its value not stored for feedback
};

}  // namespace adlplug::opl3
