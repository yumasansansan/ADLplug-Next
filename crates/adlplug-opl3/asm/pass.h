// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// The passes of ADLplug-Next OPL3's core written by hand: for AVX2, for the
// processors without AVX-VNNI and for those with it (x86_64/pass_avx2.S), for
// AVX-512 (x86_64/pass_avx512.S), and for NEON, SHA3, SVE and SVE2
// (aarch64/pass_neon.S). Where they find what they read and write,
// in the lanes of the core and in what the core gives them for a pass. The
// assembly takes the offsets from here, and src/pass.rs checks them, as it is
// compiled, against the structures the Rust code lays out.

#pragma once

// Byte offsets in the lanes: their values, the envelopes in 16 bits, the
// carries, the rhythm's bits, the shared feedback history and the phase
// increments.
#define ADLPLUG_OPL3_RATES 0
#define ADLPLUG_OPL3_KEY_SCALE_LOW 384
#define ADLPLUG_OPL3_SUSTAIN 576
#define ADLPLUG_OPL3_KEY_ON 768
#define ADLPLUG_OPL3_LEVEL_OFFSET 960
#define ADLPLUG_OPL3_TREMOLO_DEPTH 1152
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
#define ADLPLUG_OPL3_PHASE 5952
#define ADLPLUG_OPL3_OUT 6528
#define ADLPLUG_OPL3_OUT_1 6720
#define ADLPLUG_OPL3_OUT_2 6912
#define ADLPLUG_OPL3_EG_OUT 7104
#define ADLPLUG_OPL3_PHASE_OUT 7296
#define ADLPLUG_OPL3_LEVEL16 7680
#define ADLPLUG_OPL3_STATE16 7776
#define ADLPLUG_OPL3_CARRY_SOFT 7872
#define ADLPLUG_OPL3_CARRY 7912
#define ADLPLUG_OPL3_RHYTHM_BITS 7924
#define ADLPLUG_OPL3_SHARED_HISTORY 7948
#define ADLPLUG_OPL3_INCREMENT 8000

// Byte offsets in what a pass goes by besides the lanes.
#define ADLPLUG_OPL3_IN_HIGH_FROM_LESS_1 0
#define ADLPLUG_OPL3_IN_ADD 32
#define ADLPLUG_OPL3_IN_TREMOLO 64
#define ADLPLUG_OPL3_IN_EG_SECOND 96
#define ADLPLUG_OPL3_IN_RHYTHM_13 100
#define ADLPLUG_OPL3_IN_RHYTHM_16 104
#define ADLPLUG_OPL3_IN_RHYTHM_17 108
#define ADLPLUG_OPL3_IN_HOLD_13 112
#define ADLPLUG_OPL3_IN_HOLD_14 116
#define ADLPLUG_OPL3_IN_SOFT_PAN 120
#define ADLPLUG_OPL3_IN_HI_HAT_NOISE 124
#define ADLPLUG_OPL3_IN_SNARE_NOISE 128
#define ADLPLUG_OPL3_IN_LOGSIN 136
#define ADLPLUG_OPL3_IN_EXP 144
#define ADLPLUG_OPL3_IN_LOGSIN16 152
#define ADLPLUG_OPL3_IN_EXP16 160
#define ADLPLUG_OPL3_IN_LOGSIN8 168
#define ADLPLUG_OPL3_IN_EXP8 176
