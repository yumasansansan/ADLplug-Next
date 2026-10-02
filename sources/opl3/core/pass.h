// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// The passes of ADLplug-Next OPL3's core written by hand: for AVX2, for the
// processors without AVX-VNNI and for those with it (pass_avx2.S), and for
// AVX-512 (pass_avx512.S). Where they find what they read and write, in the
// core and in what core.cc gives them for a pass, and the functions themselves.
// The assembly takes the offsets from here, and core.cc checks them against the
// core as it is compiled.

#pragma once

// Byte offsets in Core: the lanes' values, the envelopes in 16 bits, the
// carries, the rhythm's bits and the shared feedback history.
#define ADLPLUG_OPL3_RATES 0
#define ADLPLUG_OPL3_KEY_SCALE_LOW 384
#define ADLPLUG_OPL3_SUSTAIN 576
#define ADLPLUG_OPL3_KEY_ON 768
#define ADLPLUG_OPL3_LEVEL_OFFSET 960
#define ADLPLUG_OPL3_TREMOLO_DEPTH 1152
#define ADLPLUG_OPL3_FNUM 1344
#define ADLPLUG_OPL3_BLOCK 1536
#define ADLPLUG_OPL3_MULTIPLE 1728
#define ADLPLUG_OPL3_VIBRATO 1920
#define ADLPLUG_OPL3_VIBRATO_DEPTH 2112
#define ADLPLUG_OPL3_PHASE_SHIFT 2304
#define ADLPLUG_OPL3_TURN_BIT 2496
#define ADLPLUG_OPL3_MUTE_BIT 2688
#define ADLPLUG_OPL3_SIGN_BIT 2880
#define ADLPLUG_OPL3_SQUARE 3072
#define ADLPLUG_OPL3_SAWTOOTH 3264
#define ADLPLUG_OPL3_MODULATED_BY 3456
#define ADLPLUG_OPL3_FEEDBACK_SHIFT 3648
#define ADLPLUG_OPL3_A_NOW 3840
#define ADLPLUG_OPL3_A_LATE 4032
#define ADLPLUG_OPL3_B_NOW 4224
#define ADLPLUG_OPL3_B_LATE 4416
#define ADLPLUG_OPL3_SOFT_A_NOW 4608
#define ADLPLUG_OPL3_SOFT_A_LATE 4800
#define ADLPLUG_OPL3_SOFT_B_NOW 4992
#define ADLPLUG_OPL3_SOFT_B_LATE 5184
#define ADLPLUG_OPL3_PHASE 6336
#define ADLPLUG_OPL3_OUT 6912
#define ADLPLUG_OPL3_OUT_1 7104
#define ADLPLUG_OPL3_OUT_2 7296
#define ADLPLUG_OPL3_EG_OUT 7488
#define ADLPLUG_OPL3_PHASE_OUT 7680
#define ADLPLUG_OPL3_LEVEL16 8064
#define ADLPLUG_OPL3_STATE16 8160
#define ADLPLUG_OPL3_CARRY_SOFT 8256
#define ADLPLUG_OPL3_CARRY 8296
#define ADLPLUG_OPL3_RHYTHM_BITS 8308
#define ADLPLUG_OPL3_SHARED_HISTORY 8332

// Byte offsets in Pass_inputs.
#define ADLPLUG_OPL3_IN_HIGH_FROM_LESS_1 0
#define ADLPLUG_OPL3_IN_ADD 32
#define ADLPLUG_OPL3_IN_TREMOLO 64
#define ADLPLUG_OPL3_IN_VIBRATO_SHIFT 96
#define ADLPLUG_OPL3_IN_VIBRATO_MASK 128
#define ADLPLUG_OPL3_IN_VIBRATO_SIGN 160
#define ADLPLUG_OPL3_IN_VIBRATO_WRAP 192
#define ADLPLUG_OPL3_IN_EG_SECOND 224
#define ADLPLUG_OPL3_IN_RHYTHM_13 228
#define ADLPLUG_OPL3_IN_RHYTHM_16 232
#define ADLPLUG_OPL3_IN_RHYTHM_17 236
#define ADLPLUG_OPL3_IN_HOLD_13 240
#define ADLPLUG_OPL3_IN_HOLD_14 244
#define ADLPLUG_OPL3_IN_SOFT_PAN 248
#define ADLPLUG_OPL3_IN_HI_HAT_NOISE 252
#define ADLPLUG_OPL3_IN_SNARE_NOISE 256
#define ADLPLUG_OPL3_IN_LOGSIN 264
#define ADLPLUG_OPL3_IN_EXP 272
#define ADLPLUG_OPL3_IN_LOGSIN16 280
#define ADLPLUG_OPL3_IN_EXP16 288
#define ADLPLUG_OPL3_IN_LOGSIN8 296
#define ADLPLUG_OPL3_IN_EXP8 304

#if !defined(__ASSEMBLER__)

#include <cstdint>

namespace adlplug::opl3 {

class Core;

// What a pass goes by besides the core: vectors of lanes, in 16 bits for the
// envelope, then single values.
struct alignas(32) Pass_inputs {
    std::int16_t high_from_less_1[16];  // the key scaling from which the timer's high step falls, less 1
    std::int16_t add[16];               // the step the timer's lowest set bit gives
    std::uint8_t tremolo[32];           // by the tremolo's depth (0 to 2): nought, the shallow, the deep; twice
    std::int32_t vibrato_shift[8];      // the shallow vibrato's shift; the deep's is one less
    std::int32_t vibrato_mask[8];       // the deep vibrato's bits; the shallow's are half of them
    std::int32_t vibrato_sign[8];       // -1 when the vibrato takes off the frequency, else 1
    std::int32_t vibrato_wrap[8];       // 1023 when it takes off, else all ones
    std::int32_t eg_second;             // the envelope timer's second half is on
    std::int32_t rhythm_13, rhythm_16, rhythm_17;  // the hi-hat, the snare and the top cymbal take phases of their own
    std::int32_t hold_13, hold_14;      // the values of slots 13 and 14 are held out of their feedback queues
    std::int32_t soft_pan;
    std::int32_t hi_hat_noise, snare_noise;
    const std::int32_t *logsin;         // the log-sine ROM
    const std::int32_t *exp;            // the exponent ROM with the 0x400 above it, shifted up one
    const std::uint16_t *logsin16;      // both in 16 bits, for the pass for AVX-512
    const std::uint16_t *exp16;
    const std::uint8_t *logsin8;        // both as their low bytes, then their high bytes, for its forms with VBMI
    const std::uint8_t *exp8;
};

// One pass over the 48 lanes, as Core::generate() works it out between taking
// its writes in and moving its counters on: for the processors without AVX-VNNI,
// which look tables up one lane at a time, and for those with it, which gather
// them and add up the outputs with AVX-VNNI's instructions.
extern "C" void adlplug_opl3_pass_avx2(Core *core, const Pass_inputs *inputs, std::int32_t *frame);
extern "C" void adlplug_opl3_pass_avx2_vnni(Core *core, const Pass_inputs *inputs, std::int32_t *frame);
// The same pass for AVX-512 (pass_avx512.S): with its foundation alone, adding
// up with AVX-512 VNNI, looking the tables up by VBMI, and with both.
extern "C" void adlplug_opl3_pass_avx512(Core *core, const Pass_inputs *inputs, std::int32_t *frame);
extern "C" void adlplug_opl3_pass_avx512_vnni(Core *core, const Pass_inputs *inputs, std::int32_t *frame);
extern "C" void adlplug_opl3_pass_avx512_vbmi(Core *core, const Pass_inputs *inputs, std::int32_t *frame);
extern "C" void adlplug_opl3_pass_avx512_vbmi_vnni(Core *core, const Pass_inputs *inputs, std::int32_t *frame);

}  // namespace adlplug::opl3

#endif
