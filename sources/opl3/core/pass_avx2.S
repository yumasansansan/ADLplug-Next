//
// Copyright (C) 2023 nukeykt
//
// This file is part of YMF262-LLE.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//  YMF262 emulator
//  Thanks:
//      John McMaster (siliconpr0n.org):
//          YMF262 decap and die shot
//
// SPDX-FileCopyrightText: 2023 nukeykt
// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later AND GPL-3.0-or-later
//
// This file is part of ADLplug-Next. It is a pass of ADLplug-Next OPL3's core
// (core.cc) written by hand for AVX2, and what it works out for a slot is what
// ymf262_logic.h works out: the envelope, the phase, the operator, the
// modulation and the rhythm phases, which follow the code of YMF262-LLE by
// nukeykt, an emulator of the YMF262 (OPL3) drawn from a die shot of the chip.
// The notice at the top is that one's, as the file stands in libADLMIDI
// (nuked_fmopl3.c, under the GNU General Public License, version 2 or any later
// version, LICENSES/GPL-2.0-or-later.txt); the counters that ymf262_logic.h
// takes from Nuked OPL3 stay in core.cc, which hands this pass what they give.
// The SPDX lines name the copyright holders and licenses in the
// machine-readable form of the REUSE specification, with ADLplug-Next's code
// under the GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
//     void adlplug_opl3_pass_avx2(Core *core, const Pass_inputs *inputs,
//                                 std::int32_t *frame);
//     void adlplug_opl3_pass_avx2_vnni(Core *core, const Pass_inputs *inputs,
//                                      std::int32_t *frame);
//
// Each does what Core::generate() does between taking its writes in and moving
// its counters on (pass_avx2.h): the envelopes and the phases, the rhythm
// phases, the operators stage by stage, the feedback history and the sums of
// the two outputs. The first needs AVX2, which the build asks for, the second
// AVX-VNNI as well, which core.cc asks the processor for: Intel's have it from
// Alder Lake on, AMD's from Zen 5.
//
// The lanes are those of the core, eight to a vector of 32 bits, but for the
// envelopes, which are worked out sixteen to a vector of 16 bits: every value
// they go by fits, and so twice as many lanes go through each instruction. AVX2
// has no shift by a count for each 16-bit lane, so the two that the envelope
// needs are a small table (vpshufb) and a multiplication by a power of two. The
// stages of operators wait for each other, through two table lookups each, so
// each stage starts with what does not wait for the stage before, and the
// envelopes and phases of the next stage come between stages, for the
// processor to work on while it waits.
//
// The two passes differ in the lookups and in the sums of the outputs, and are
// assembled from the same macros. Measured on GitHub's runners and on the
// i7-1360P here, the processors with AVX-VNNI (Raptor Lake, Emerald Rapids,
// Granite Rapids, Zen 5) gather fast, and the second pass looks tables up by
// vpgatherdd and adds up with AVX-VNNI's multiplications that add into the sums
// (vpdpwssd). Those without it (Ice Lake under the microcode that mitigates
// Downfall, where a gather waits 42 cycles and they go no faster than one in
// 28, Zen 3, Zen 4) look tables up as fast or faster by loads one a lane, as
// the earlier processors of both makers, whose gathers are slower still, are
// to; the first pass loads the eight lanes one by one, and adds up with
// multiplications that add pairs (vpmaddwd) and additions.
//
// Assembled by Clang, in Intel's syntax, for ELF (Linux) and COFF (Windows).

#include "pass_avx2.h"

        .intel_syntax noprefix

        .set RATES, ADLPLUG_OPL3_RATES
        .set KEY_SCALE_LOW, ADLPLUG_OPL3_KEY_SCALE_LOW
        .set SUSTAIN, ADLPLUG_OPL3_SUSTAIN
        .set KEY_ON, ADLPLUG_OPL3_KEY_ON
        .set LEVEL_OFFSET, ADLPLUG_OPL3_LEVEL_OFFSET
        .set TREMOLO_DEPTH, ADLPLUG_OPL3_TREMOLO_DEPTH
        .set FNUM, ADLPLUG_OPL3_FNUM
        .set BLOCK, ADLPLUG_OPL3_BLOCK
        .set MULTIPLE, ADLPLUG_OPL3_MULTIPLE
        .set VIBRATO, ADLPLUG_OPL3_VIBRATO
        .set VIBRATO_DEPTH, ADLPLUG_OPL3_VIBRATO_DEPTH
        .set PHASE_SHIFT, ADLPLUG_OPL3_PHASE_SHIFT
        .set TURN_BIT, ADLPLUG_OPL3_TURN_BIT
        .set MUTE_BIT, ADLPLUG_OPL3_MUTE_BIT
        .set SIGN_BIT, ADLPLUG_OPL3_SIGN_BIT
        .set SQUARE, ADLPLUG_OPL3_SQUARE
        .set SAWTOOTH, ADLPLUG_OPL3_SAWTOOTH
        .set MODULATED_BY, ADLPLUG_OPL3_MODULATED_BY
        .set FEEDBACK_SHIFT, ADLPLUG_OPL3_FEEDBACK_SHIFT
        .set A_NOW, ADLPLUG_OPL3_A_NOW
        .set A_LATE, ADLPLUG_OPL3_A_LATE
        .set B_NOW, ADLPLUG_OPL3_B_NOW
        .set B_LATE, ADLPLUG_OPL3_B_LATE
        .set SOFT_A_NOW, ADLPLUG_OPL3_SOFT_A_NOW
        .set SOFT_A_LATE, ADLPLUG_OPL3_SOFT_A_LATE
        .set SOFT_B_NOW, ADLPLUG_OPL3_SOFT_B_NOW
        .set SOFT_B_LATE, ADLPLUG_OPL3_SOFT_B_LATE
        .set PHASE, ADLPLUG_OPL3_PHASE
        .set OUT, ADLPLUG_OPL3_OUT
        .set OUT_1, ADLPLUG_OPL3_OUT_1
        .set OUT_2, ADLPLUG_OPL3_OUT_2
        .set EG_OUT, ADLPLUG_OPL3_EG_OUT
        .set PHASE_OUT, ADLPLUG_OPL3_PHASE_OUT
        .set LEVEL16, ADLPLUG_OPL3_LEVEL16
        .set STATE16, ADLPLUG_OPL3_STATE16
        .set CARRY_SOFT, ADLPLUG_OPL3_CARRY_SOFT             // a, then b, 64 bits each
        .set CARRY, ADLPLUG_OPL3_CARRY                       // a, then b, 32 bits each
        .set HH2, ADLPLUG_OPL3_RHYTHM_BITS
        .set HH3, ADLPLUG_OPL3_RHYTHM_BITS + 4
        .set HH7, ADLPLUG_OPL3_RHYTHM_BITS + 8
        .set HH8, ADLPLUG_OPL3_RHYTHM_BITS + 12
        .set TC3, ADLPLUG_OPL3_RHYTHM_BITS + 16
        .set TC5, ADLPLUG_OPL3_RHYTHM_BITS + 20
        .set SHARED_HISTORY, ADLPLUG_OPL3_SHARED_HISTORY     // two queues of four

        .set IN_HIGH_FROM_LESS_1, ADLPLUG_OPL3_IN_HIGH_FROM_LESS_1
        .set IN_ADD, ADLPLUG_OPL3_IN_ADD
        .set IN_TREMOLO, ADLPLUG_OPL3_IN_TREMOLO
        .set IN_VIBRATO_SHIFT, ADLPLUG_OPL3_IN_VIBRATO_SHIFT
        .set IN_VIBRATO_MASK, ADLPLUG_OPL3_IN_VIBRATO_MASK
        .set IN_VIBRATO_SIGN, ADLPLUG_OPL3_IN_VIBRATO_SIGN
        .set IN_VIBRATO_WRAP, ADLPLUG_OPL3_IN_VIBRATO_WRAP
        .set IN_EG_SECOND, ADLPLUG_OPL3_IN_EG_SECOND
        .set IN_RHYTHM_13, ADLPLUG_OPL3_IN_RHYTHM_13
        .set IN_RHYTHM_16, ADLPLUG_OPL3_IN_RHYTHM_16
        .set IN_RHYTHM_17, ADLPLUG_OPL3_IN_RHYTHM_17
        .set IN_HOLD_13, ADLPLUG_OPL3_IN_HOLD_13
        .set IN_HOLD_14, ADLPLUG_OPL3_IN_HOLD_14
        .set IN_SOFT_PAN, ADLPLUG_OPL3_IN_SOFT_PAN
        .set IN_HI_HAT_NOISE, ADLPLUG_OPL3_IN_HI_HAT_NOISE
        .set IN_SNARE_NOISE, ADLPLUG_OPL3_IN_SNARE_NOISE
        .set IN_LOGSIN, ADLPLUG_OPL3_IN_LOGSIN
        .set IN_EXP, ADLPLUG_OPL3_IN_EXP

#if defined(_WIN32)
        .section .rdata, "dr"
#else
        .section .rodata
#endif
        .p2align 5
// Vectors of eight 32-bit lanes.
.Lc_1:          .rept 8
                .long 1
                .endr
.Lc_2:          .rept 8
                .long 2
                .endr
.Lc_255:        .rept 8
                .long 255
                .endr
.Lc_511:        .rept 8
                .long 511
                .endr
.Lc_1023:       .rept 8
                .long 1023
                .endr
.Lc_0x7ffff:    .rept 8
                .long 0x7ffff
                .endr
.Lc_0x7fff:     .rept 8
                .long 0x7fff
                .endr
.Lc_32767:      .rept 8
                .long 32767
                .endr
.Lc_minus_32768: .rept 8
                .long -32768
                .endr
// Vectors of sixteen 16-bit lanes.
.Lw_1:          .rept 16
                .short 1
                .endr
.Lw_3:          .rept 16
                .short 3
                .endr
.Lw_4:          .rept 16
                .short 4
                .endr
.Lw_12:         .rept 16
                .short 12
                .endr
.Lw_minus_12:   .rept 16
                .short -12
                .endr
.Lw_15:         .rept 16
                .short 15
                .endr
.Lw_16:         .rept 16
                .short 16
                .endr
.Lw_255:        .rept 16
                .short 255
                .endr
.Lw_0x1f7:      .rept 16
                .short 0x1f7
                .endr
.Lw_0x1ff:      .rept 16
                .short 0x1ff
                .endr
.Lw_0x200:      .rept 16
                .short 0x200
                .endr
// vpshufb's tables, one to each half of a vector, and the bytes that put a
// lane's index into them: the index in the low byte, and in the high byte an
// index of its own, or 0x80 for nought.
//
// By the sum of a rate and the timer's step (0 to 15): the key scalings, a bit
// for each value of their low two bits, for which the low step falls.
.Lb_low_step:   .rept 2
                .byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x0f, 0x0c, 0x0a, 0
                .endr
// By a step (0 to 3): the low byte of the attack's high bits, and at 4 their
// 0x100, which the index's high byte takes.
.Lb_attack_bits: .rept 2
                .byte 0x40, 0xc0, 0x80, 0x00, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
                .endr
.Lw_attack_bits_index: .rept 16
                .short 0x0400
                .endr
// By 0 to 3: 1 shifted up by it.
.Lb_pow2:       .rept 2
                .byte 1, 2, 4, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
                .endr
.Lw_pow2_index: .rept 16
                .short 0x8000
                .endr

        .text

// The 32-bit values of the sixteen lanes at byte \g, two vectors, in 16 bits, in
// the order vpackusdw leaves them in: the first vector's lanes 0 to 3, the
// second's 0 to 3, the first's 4 to 7, the second's 4 to 7. The envelopes keep
// their levels and states in that order too.
.macro PACK dst, offset, g
        vmovdqa   \dst, ymmword ptr [r8 + \offset + \g]
        vpackusdw \dst, \dst, ymmword ptr [r8 + \offset + \g + 32]
.endm

// The envelopes of the sixteen lanes at byte \g (ymf262::envelope() and
// attenuation()): the state and the level for the next pass, and what the
// operator takes of this pass's level. The rate a state goes by is taken from
// its byte in 32 bits. Leaves dokon (keyed on from release) in 32 bits for the
// phases: the first vector's in ymm3, the second's in ymm8. \second is the
// envelope timer's second half. ymm15 holds nought.
.macro ENVELOPE second, g
        vmovdqa   ymm0, ymmword ptr [r8 + STATE16 + \g / 2]
        vmovdqa   ymm1, ymmword ptr [r8 + LEVEL16 + \g / 2]
        PACK      ymm2, KEY_ON, \g
        vpsubw    ymm2, ymm15, ymm2                                   // keyed: all ones
        vpcmpeqw  ymm3, ymm0, ymmword ptr [rip + .Lw_3]
        vpand     ymm3, ymm3, ymm2                                    // dokon
        vpandn    ymm4, ymm3, ymm0                                    // the state the rate goes by
        vpunpcklwd ymm5, ymm4, ymm15
        vpunpckhwd ymm4, ymm4, ymm15
        vpslld    ymm5, ymm5, 3
        vpslld    ymm4, ymm4, 3
        vmovdqa   ymm6, ymmword ptr [r8 + RATES + \g]
        vpsrlvd   ymm5, ymm6, ymm5
        vmovdqa   ymm6, ymmword ptr [r8 + RATES + \g + 32]
        vpsrlvd   ymm4, ymm6, ymm4
        vpand     ymm5, ymm5, ymmword ptr [rip + .Lc_255]
        vpand     ymm4, ymm4, ymmword ptr [rip + .Lc_255]
        vpackusdw ymm4, ymm5, ymm4                                    // its rate
        vpand     ymm5, ymm4, ymmword ptr [rip + .Lw_15]             // rate_hi
        PACK      ymm6, KEY_SCALE_LOW, \g
        vpcmpgtw  ymm7, ymm6, ymmword ptr [r9 + IN_HIGH_FROM_LESS_1] // stephi
.if \second
        vpcmpeqw  ymm8, ymm5, ymmword ptr [rip + .Lw_12]
        vpor      ymm7, ymm7, ymm8                                    // high
.endif
        vpaddw    ymm8, ymm5, ymmword ptr [rip + .Lw_minus_12]
        vpsubw    ymm8, ymm8, ymm7
        vpminsw   ymm8, ymm8, ymmword ptr [rip + .Lw_3]              // the step from rate 12, none below
.if \second
        // The low step, below rate 12. Rates of 12 and up take a step of 1 at
        // least when the timer's second half is on, which the low step cannot
        // raise, so it is not asked whether the rate is below 12.
        vpaddw    ymm7, ymm5, ymmword ptr [r9 + IN_ADD]
        vpand     ymm7, ymm7, ymmword ptr [rip + .Lw_15]             // sum
        vmovdqa   ymm9, ymmword ptr [rip + .Lb_low_step]
        vpshufb   ymm7, ymm9, ymm7
        vmovdqa   ymm9, ymmword ptr [rip + .Lb_pow2]
        vpshufb   ymm6, ymm9, ymm6                                    // this key scaling's bit, and a byte above
        vpand     ymm4, ymm4, ymmword ptr [rip + .Lw_16]
        vpcmpeqw  ymm4, ymm4, ymmword ptr [rip + .Lw_16]             // the register's rate is not 0
        vpand     ymm6, ymm6, ymm4
        vpand     ymm7, ymm7, ymm6
        vpminuw   ymm7, ymm7, ymmword ptr [rip + .Lw_1]              // inclow
        vpmaxsw   ymm8, ymm8, ymm7                                    // step
.else
        vpmaxsw   ymm8, ymm8, ymm15                                   // step
.endif
        vpcmpeqw  ymm9, ymm5, ymmword ptr [rip + .Lw_15]             // maxrate
        vpcmpeqw  ymm4, ymm0, ymm15                                   // attack
        vpcmpeqw  ymm5, ymm1, ymm15                                   // zeroreach
        PACK      ymm6, SUSTAIN, \g
        vpsrlw    ymm7, ymm1, 4
        vpcmpeqw  ymm6, ymm6, ymm7                                    // slreach
        vpcmpgtw  ymm10, ymm1, ymmword ptr [rip + .Lw_0x1f7]         // silent
        vpcmpeqw  ymm11, ymm0, ymmword ptr [rip + .Lw_1]             // decay
        // The state for the next pass: one on from attack at level 0 and from
        // decay at the sustain level, release with the key off, attack on dokon.
        vpand     ymm12, ymm11, ymm6
        vpand     ymm13, ymm4, ymm5
        vpor      ymm12, ymm12, ymm13
        vpsubw    ymm12, ymm0, ymm12
        vpandn    ymm13, ymm2, ymmword ptr [rip + .Lw_3]
        vpor      ymm12, ymm12, ymm13
        vpandn    ymm12, ymm3, ymm12
        vmovdqa   ymmword ptr [r8 + STATE16 + \g / 2], ymm12
        vpcmpgtw  ymm12, ymm0, ymmword ptr [rip + .Lw_1]             // sustain or release
        vpandn    ymm13, ymm6, ymm11                                  // decay short of the sustain level
        vpor      ymm12, ymm12, ymm13
        vpor      ymm13, ymm3, ymm10
        vpandn    ymm12, ymm13, ymm12                                 // linear
        vpand     ymm13, ymm4, ymm2
        vpor      ymm14, ymm9, ymm5
        vpandn    ymm13, ymm14, ymm13                                 // exponent
        vpand     ymm9, ymm9, ymm3                                    // instant
        vpor      ymm4, ymm4, ymm3
        vpandn    ymm4, ymm4, ymm10                                   // mute
        vpandn    ymm14, ymm9, ymm1
        vpand     ymm4, ymm4, ymmword ptr [rip + .Lw_0x1ff]
        vpor      ymm14, ymm14, ymm4                                  // the level before the step
        vpsrlw    ymm4, ymm1, 1
        vpxor     ymm4, ymm4, ymmword ptr [rip + .Lw_255]
        vpand     ymm4, ymm4, ymm13
        vpand     ymm12, ymm12, ymmword ptr [rip + .Lw_4]
        vpor      ymm4, ymm4, ymm12                                   // the increment
        vpor      ymm12, ymm8, ymmword ptr [rip + .Lw_attack_bits_index]
        vmovdqa   ymm10, ymmword ptr [rip + .Lb_attack_bits]
        vpshufb   ymm12, ymm10, ymm12
        vpand     ymm12, ymm12, ymm13                                 // the attack's high bits
        // The increment shifted down by 3 less the step: shifted up by the
        // step, a multiplication, then down by 3.
        vpor      ymm10, ymm8, ymmword ptr [rip + .Lw_pow2_index]
        vmovdqa   ymm11, ymmword ptr [rip + .Lb_pow2]
        vpshufb   ymm10, ymm11, ymm10
        vpmullw   ymm4, ymm4, ymm10
        vpsrlw    ymm4, ymm4, 3
        vpor      ymm4, ymm4, ymm12
        vpcmpeqw  ymm10, ymm8, ymm15
        vpandn    ymm4, ymm10, ymm4                                   // nothing without a step
        vpaddw    ymm4, ymm4, ymm14
        vpand     ymm4, ymm4, ymmword ptr [rip + .Lw_0x1ff]
        vmovdqa   ymmword ptr [r8 + LEVEL16 + \g / 2], ymm4
        // The attenuation the operator takes: this pass's level with the key
        // scale level, the total level and the tremolo, all ones past nine bits.
        PACK      ymm5, TREMOLO_DEPTH, \g
        vmovdqa   ymm6, ymmword ptr [r9 + IN_TREMOLO]
        vpshufb   ymm5, ymm6, ymm5                                    // the tremolo
        PACK      ymm6, LEVEL_OFFSET, \g
        vpaddw    ymm5, ymm5, ymm6
        vpand     ymm6, ymm5, ymmword ptr [rip + .Lw_0x1ff]
        vpaddw    ymm6, ymm6, ymm1
        vpand     ymm5, ymm5, ymmword ptr [rip + .Lw_0x200]
        vpor      ymm6, ymm6, ymm5
        vpminsw   ymm6, ymm6, ymmword ptr [rip + .Lw_0x1ff]
        vpunpcklwd ymm5, ymm6, ymm15
        vmovdqa   ymmword ptr [r8 + EG_OUT + \g], ymm5
        vpunpckhwd ymm6, ymm6, ymm15
        vmovdqa   ymmword ptr [r8 + EG_OUT + \g + 32], ymm6
        vpunpckhwd ymm8, ymm3, ymm3
        vpunpcklwd ymm3, ymm3, ymm3
.endm

// The phases of the eight lanes at byte \v (ymf262::phase_increment(),
// phase_out() and next_phase()): what the operator reads of this pass's phase,
// and the phase for the next pass, from nought on dokon (\restart).
.macro PHASE v, restart
        vmovdqa   ymm4, ymmword ptr [r8 + VIBRATO_DEPTH + \v]
        vmovdqa   ymm5, ymmword ptr [r8 + FNUM + \v]
        vpsllvd   ymm6, ymm5, ymm4
        vpsrlvd   ymm6, ymm6, ymmword ptr [r9 + IN_VIBRATO_SHIFT]   // fnum shifted down as the depth asks
        vpxor     ymm4, ymm4, ymmword ptr [rip + .Lc_1]
        vmovdqa   ymm7, ymmword ptr [r9 + IN_VIBRATO_MASK]
        vpsrlvd   ymm4, ymm7, ymm4                                    // the depth's bits
        vpand     ymm6, ymm6, ymm4
        vpsubd    ymm4, ymm15, ymmword ptr [r8 + VIBRATO + \v]
        vpand     ymm6, ymm6, ymm4                                    // the vibrato's swing
        vpsignd   ymm6, ymm6, ymmword ptr [r9 + IN_VIBRATO_SIGN]
        vpaddd    ymm5, ymm5, ymm6
        vpand     ymm5, ymm5, ymmword ptr [r9 + IN_VIBRATO_WRAP]    // the frequency number
        vpsllvd   ymm5, ymm5, ymmword ptr [r8 + BLOCK + \v]
        vpsrld    ymm5, ymm5, 1
        vpmulld   ymm5, ymm5, ymmword ptr [r8 + MULTIPLE + \v]
        vpsrld    ymm5, ymm5, 1                                       // the increment
        vmovdqa   ymm6, ymmword ptr [r8 + PHASE + \v]
        vpsrld    ymm7, ymm6, 9
        vmovdqa   ymmword ptr [r8 + PHASE_OUT + \v], ymm7
        vpandn    ymm6, \restart, ymm6
        vpaddd    ymm6, ymm6, ymm5
        vpand     ymm6, ymm6, ymmword ptr [rip + .Lc_0x7ffff]
        vmovdqa   ymmword ptr [r8 + PHASE + \v], ymm6
.endm

.macro ENVELOPES_AND_PHASES second, g
        ENVELOPE  \second, \g
        PHASE     \g, ymm3
        PHASE     \g + 32, ymm8
.endm

// The eight lanes of \idx looked up in the table at \table, into \dst, \dstx and
// \idxx being the low halves of the two: by vpgatherdd in the pass for the
// processors with AVX-VNNI (\vnni 1), with ymm6 for its mask; in the other one
// by one, with rax, r11, xmm12 and xmm13, which the operators leave free.
.macro LOOKUP vnni, dst, dstx, idx, idxx, table
.if \vnni
        vpcmpeqd  ymm6, ymm6, ymm6
        vpxor     \dstx, \dstx, \dstx
        vpgatherdd \dst, dword ptr [\table + 4*\idx], ymm6
.else
        vmovd     eax, \idxx
        vpextrd   r11d, \idxx, 1
        vmovd     \dstx, dword ptr [\table + 4*rax]
        vpinsrd   \dstx, \dstx, dword ptr [\table + 4*r11], 1
        vpextrd   eax, \idxx, 2
        vpextrd   r11d, \idxx, 3
        vpinsrd   \dstx, \dstx, dword ptr [\table + 4*rax], 2
        vpinsrd   \dstx, \dstx, dword ptr [\table + 4*r11], 3
        vextracti128 xmm12, \idx, 1
        vmovd     eax, xmm12
        vpextrd   r11d, xmm12, 1
        vmovd     xmm13, dword ptr [\table + 4*rax]
        vpinsrd   xmm13, xmm13, dword ptr [\table + 4*r11], 1
        vpextrd   eax, xmm12, 2
        vpextrd   r11d, xmm12, 3
        vpinsrd   xmm13, xmm13, dword ptr [\table + 4*rax], 2
        vpinsrd   xmm13, xmm13, dword ptr [\table + 4*r11], 3
        vinserti128 \dst, \dst, xmm13, 1
.endif
.endm

// The operators of the eight lanes at byte \v (ymf262::modulation() and
// operator_output()), modulated, when they are by the stage before, by the
// lanes at byte \source. rcx and rdx point at the tables of the log-sine and of
// the exponent.
.macro OPERATOR v, source, vnni
        // First what does not wait for the stage before: the feedback, added to
        // the phase as the modulation, to which the stage before's modulation is
        // added later (no slot has both).
        vmovdqa   ymm0, ymmword ptr [r8 + MODULATED_BY + \v]
        vpcmpeqd  ymm1, ymm0, ymmword ptr [rip + .Lc_1]
        vpand     ymm1, ymm1, ymmword ptr [rip + .Lc_1023]           // what is taken of the stage before
        vpcmpeqd  ymm0, ymm0, ymmword ptr [rip + .Lc_2]
        vmovdqa   ymm2, ymmword ptr [r8 + OUT_1 + \v]
        vpaddd    ymm2, ymm2, ymmword ptr [r8 + OUT_2 + \v]
        vpsravd   ymm2, ymm2, ymmword ptr [r8 + FEEDBACK_SHIFT + \v]
        vpand     ymm2, ymm2, ymm0
        vpand     ymm2, ymm2, ymmword ptr [rip + .Lc_1023]           // the feedback
        vpaddd    ymm2, ymm2, ymmword ptr [r8 + PHASE_OUT + \v]
        vmovdqa   ymm5, ymmword ptr [r8 + TURN_BIT + \v]
        vpsubd    ymm3, ymm15, ymmword ptr [r8 + SQUARE + \v]        // square: all ones
        // From the stage before to the log-sine's index.
        vpand     ymm1, ymm1, ymmword ptr [r8 + OUT + \source]
        vpaddd    ymm1, ymm1, ymm2                                    // the phase
        vpsllvd   ymm2, ymm1, ymmword ptr [r8 + PHASE_SHIFT + \v]    // shifted, with the bits above its ten
        vpand     ymm4, ymm2, ymm5
        vpcmpeqd  ymm4, ymm4, ymm5                                    // turned back: all ones
        vpxor     ymm4, ymm4, ymm2                                    // phase2, in its low nine bits
        vpor      ymm5, ymm4, ymm3
        vpand     ymm5, ymm5, ymmword ptr [rip + .Lc_255]            // the log-sine's index
        LOOKUP    \vnni, ymm7, xmm7, ymm5, xmm5, rcx                  // sine
        // While it loads: the ramp, the sign (bit 9 of shifted, which turning
        // back leaves alone) and the mute.
        vpsubd    ymm6, ymm15, ymmword ptr [r8 + SAWTOOTH + \v]      // sawtooth: all ones
        vpand     ymm4, ymm4, ymmword ptr [rip + .Lc_511]
        vpslld    ymm4, ymm4, 3
        vpand     ymm4, ymm4, ymm6                                    // the ramp, for the sawtooth
        vpand     ymm2, ymm2, ymmword ptr [r8 + SIGN_BIT + \v]
        vpcmpeqd  ymm2, ymm2, ymm15                                   // positive
        vpand     ymm1, ymm1, ymmword ptr [r8 + MUTE_BIT + \v]
        vpcmpeqd  ymm1, ymm1, ymm15                                   // heard
        vpandn    ymm2, ymm2, ymm1                                    // heard and negative
        vmovdqa   ymm0, ymmword ptr [r8 + EG_OUT + \v]
        vpslld    ymm0, ymm0, 3
        // From the sine to the exponent's index. att is not held to 4095: past
        // it, it shifts the magnitude out all the same.
        vpandn    ymm7, ymm6, ymm7
        vpor      ymm7, ymm7, ymm4                                    // shape
        vpaddd    ymm7, ymm7, ymm0                                    // att
        vpand     ymm3, ymm7, ymmword ptr [rip + .Lc_255]
        LOOKUP    \vnni, ymm5, xmm5, ymm3, xmm3, rdx
        vpsrld    ymm7, ymm7, 8
        vpsrlvd   ymm5, ymm5, ymm7                                    // magnitude, below 4096
        // The value: nought when muted, and when negative the magnitude's
        // complement, which is the 13-bit ones' complement taken as signed.
        vpand     ymm5, ymm5, ymm1
        vpxor     ymm5, ymm5, ymm2
        vmovdqa   ymmword ptr [r8 + OUT + \v], ymm5
.endm

// The ring of the hi-hat's and the top cymbal's bits, into \r, with \t.
.macro RING r, t
        mov       \r, dword ptr [r8 + HH2]
        xor       \r, dword ptr [r8 + HH7]
        mov       \t, dword ptr [r8 + TC5]
        xor       \t, dword ptr [r8 + HH3]
        or        \r, \t
        mov       \t, dword ptr [r8 + TC5]
        xor       \t, dword ptr [r8 + TC3]
        or        \r, \t
.endm

// The hi-hat (slot 13, lane 7, Core::rhythm_phases()): its bits are taken every
// pass, and in rhythm mode it takes a phase of its own, by the top cymbal's bits
// of the last time they were taken.
.macro HI_HAT
        mov       eax, dword ptr [r8 + PHASE_OUT + 28]
        mov       r11d, eax
        shr       r11d, 2
        and       r11d, 1
        mov       dword ptr [r8 + HH2], r11d
        mov       r11d, eax
        shr       r11d, 3
        and       r11d, 1
        mov       dword ptr [r8 + HH3], r11d
        mov       r11d, eax
        shr       r11d, 7
        and       r11d, 1
        mov       dword ptr [r8 + HH7], r11d
        shr       eax, 8
        and       eax, 1
        mov       dword ptr [r8 + HH8], eax
        cmp       dword ptr [r9 + IN_RHYTHM_13], 0
        je        1f
        RING      eax, r11d
        mov       r11d, eax
        xor       r11d, dword ptr [r9 + IN_HI_HAT_NOISE]
        imul      r11d, r11d, 0xd0 - 0x34
        add       r11d, 0x34
        shl       eax, 9
        or        eax, r11d
        mov       dword ptr [r8 + PHASE_OUT + 28], eax
1:
.endm

// The snare (slot 16, lane 23) and the top cymbal (slot 17, lane 24), by the
// hi-hat's bits of this pass.
.macro SNARE_AND_TOP_CYMBAL
        cmp       dword ptr [r9 + IN_RHYTHM_16], 0
        je        1f
        mov       eax, dword ptr [r8 + HH8]
        mov       r11d, eax
        xor       r11d, dword ptr [r9 + IN_SNARE_NOISE]
        shl       r11d, 8
        shl       eax, 9
        or        eax, r11d
        mov       dword ptr [r8 + PHASE_OUT + 92], eax
1:
        cmp       dword ptr [r9 + IN_RHYTHM_17], 0
        je        2f
        mov       eax, dword ptr [r8 + PHASE_OUT + 96]
        mov       r11d, eax
        shr       r11d, 3
        and       r11d, 1
        mov       dword ptr [r8 + TC3], r11d
        shr       eax, 5
        and       eax, 1
        mov       dword ptr [r8 + TC5], eax
        RING      eax, r11d
        shl       eax, 9
        or        eax, 0x80
        mov       dword ptr [r8 + PHASE_OUT + 96], eax
2:
.endm

// The feedback history of the slots that share a queue (\q: 0 for slots 13 and
// 31, lanes 7 and 10; 1 for 14 and 32, lanes 8 and 11; Core::operators()): slot
// m reads the second and the fourth value, slot m + 18 the first and the third,
// or the same two as m when m's value is held out of the queue.
.macro SHARED_READ q, low, high, hold
        mov       eax, dword ptr [r8 + SHARED_HISTORY + 16 * \q + 4]
        mov       dword ptr [r8 + OUT_1 + 4 * \low], eax
        mov       r11d, dword ptr [r8 + SHARED_HISTORY + 16 * \q]
        cmp       dword ptr [r9 + \hold], 0
        cmovne    r11d, eax
        mov       dword ptr [r8 + OUT_1 + 4 * \high], r11d
        mov       eax, dword ptr [r8 + SHARED_HISTORY + 16 * \q + 12]
        mov       dword ptr [r8 + OUT_2 + 4 * \low], eax
        mov       r11d, dword ptr [r8 + SHARED_HISTORY + 16 * \q + 8]
        cmp       dword ptr [r9 + \hold], 0
        cmovne    r11d, eax
        mov       dword ptr [r8 + OUT_2 + 4 * \high], r11d
.endm

// The values of this pass into the queue (Core::store_history()): slot m's but
// when it is held out, then slot m + 18's.
.macro SHARED_STORE q, low, high, hold
        vmovdqu   xmm0, xmmword ptr [r8 + SHARED_HISTORY + 16 * \q]
        cmp       dword ptr [r9 + \hold], 0
        jne       1f
        vpslldq   xmm0, xmm0, 4
        vpinsrd   xmm0, xmm0, dword ptr [r8 + OUT + 4 * \low], 0
1:
        vpslldq   xmm0, xmm0, 4
        vpinsrd   xmm0, xmm0, dword ptr [r8 + OUT + 4 * \high], 0
        vmovdqu   xmmword ptr [r8 + SHARED_HISTORY + 16 * \q], xmm0
.endm

// The last two values of the eight lanes at byte \v.
.macro HISTORY v
        vmovdqa   ymm0, ymmword ptr [r8 + OUT_1 + \v]
        vmovdqa   ymmword ptr [r8 + OUT_2 + \v], ymm0
        vmovdqa   ymm0, ymmword ptr [r8 + OUT + \v]
        vmovdqa   ymmword ptr [r8 + OUT_1 + \v], ymm0
.endm

// The products of the values of the eight lanes at byte \v and their weights on
// the outputs, added into ymm8 to ymm11. A value fits 16 bits and a weight is 2
// at the most, so each lane's product is that of its low 16 bits. With \vnni,
// AVX-VNNI's multiplications that add into the sums; without, multiplications
// that add pairs, then additions.
.macro HEARD v, vnni
        vmovdqa   ymm0, ymmword ptr [r8 + OUT + \v]
.if \vnni
        {vex} vpdpwssd ymm8, ymm0, ymmword ptr [r8 + A_NOW + \v]
        {vex} vpdpwssd ymm9, ymm0, ymmword ptr [r8 + A_LATE + \v]
        {vex} vpdpwssd ymm10, ymm0, ymmword ptr [r8 + B_NOW + \v]
        {vex} vpdpwssd ymm11, ymm0, ymmword ptr [r8 + B_LATE + \v]
.else
        vpmaddwd  ymm1, ymm0, ymmword ptr [r8 + A_NOW + \v]
        vpmaddwd  ymm2, ymm0, ymmword ptr [r8 + A_LATE + \v]
        vpmaddwd  ymm3, ymm0, ymmword ptr [r8 + B_NOW + \v]
        vpmaddwd  ymm4, ymm0, ymmword ptr [r8 + B_LATE + \v]
        vpaddd    ymm8, ymm8, ymm1
        vpaddd    ymm9, ymm9, ymm2
        vpaddd    ymm10, ymm10, ymm3
        vpaddd    ymm11, ymm11, ymm4
.endif
.endm

// The same with soft panning, whose weights reach 2^17: each as its low fifteen
// bits and the rest, the products of the former added into ymm8 to ymm11, of the
// latter into ymm12 to ymm15. Over the six vectors a lane's sum of the former
// stays within 2^30.
.macro HEARD_SOFT v, vnni
        vmovdqa   ymm0, ymmword ptr [r8 + OUT + \v]
        HEARD_SOFT_ONE \v, SOFT_A_NOW, ymm8, ymm12, \vnni
        HEARD_SOFT_ONE \v, SOFT_A_LATE, ymm9, ymm13, \vnni
        HEARD_SOFT_ONE \v, SOFT_B_NOW, ymm10, ymm14, \vnni
        HEARD_SOFT_ONE \v, SOFT_B_LATE, ymm11, ymm15, \vnni
.endm

.macro HEARD_SOFT_ONE v, offset, low, high, vnni
        vmovdqa   ymm1, ymmword ptr [r8 + \offset + \v]
        vpand     ymm2, ymm1, ymmword ptr [rip + .Lc_0x7fff]
        vpsrld    ymm1, ymm1, 15
.if \vnni
        {vex} vpdpwssd \low, ymm0, ymm2
        {vex} vpdpwssd \high, ymm0, ymm1
.else
        vpmaddwd  ymm2, ymm0, ymm2
        vpmaddwd  ymm1, ymm0, ymm1
        vpaddd    \low, \low, ymm2
        vpaddd    \high, \high, ymm1
.endif
.endm

// The eight 32-bit lanes of \y summed in 64 bits, into both halves of \x, the
// low half of \y, with \t and \u.
.macro SUM64 x, y, t, u
        vextracti128 \t, \y, 1
        vpmovsxdq \u, \x
        vpmovsxdq \y, \t
        vpaddq    \y, \y, \u
        vextracti128 \t, \y, 1
        vpaddq    \x, \x, \t
        vpshufd   \t, \x, 0x4e
        vpaddq    \x, \x, \t
.endm

// \r, of 64 bits, shifted down by 16 and rounded half away from nought, with
// \t.
.macro ROUND16 r, t
        mov       \t, \r
        sar       \t, 63
        xor       \r, \t
        sub       \r, \t
        add       \r, 32768
        shr       \r, 16
        xor       \r, \t
        sub       \r, \t
.endm

// \r, of 32 bits, in the nineteen bits the chip adds in, then held to sixteen,
// with \t.
.macro CLIP r, t
        shl       \r, 13
        sar       \r, 13
        mov       \t, 32767
        cmp       \r, \t
        cmovg     \r, \t
        mov       \t, -32768
        cmp       \r, \t
        cmovl     \r, \t
.endm

// The envelopes and phases and the operators, for a pass with the envelope
// timer's second half off (\second 0) or on (1), in the pass for the processors
// with AVX-VNNI (\vnni 1) or in the other. Each stage's envelopes and phases come
// before it, and the next stage's after it: stage 0 is lanes 0 to 15, stage 1
// lanes 16 to 31, modulated by stage 0, stage 2 lanes 32 to 39 and stage 3 lanes
// 40 to 47, modulated by stages 1 and 2. The hi-hat's phase comes before stage
// 0, and the snare's and the top cymbal's before stage 1.
.macro PASS second, vnni
        mov       rcx, qword ptr [r9 + IN_LOGSIN]
        mov       rdx, qword ptr [r9 + IN_EXP]
        ENVELOPES_AND_PHASES \second, 0
        HI_HAT
        OPERATOR  0, 0, \vnni
        OPERATOR  32, 32, \vnni
        ENVELOPES_AND_PHASES \second, 64
        SNARE_AND_TOP_CYMBAL
        OPERATOR  64, 0, \vnni
        OPERATOR  96, 32, \vnni
        ENVELOPES_AND_PHASES \second, 128
        OPERATOR  128, 64, \vnni
        OPERATOR  160, 128, \vnni
.endm

// The pass \name, for the processors with AVX-VNNI (\vnni 1) or for the
// others. Its labels end in \vnni.
.macro PASS_FUNCTION name, vnni
        .globl    \name
#if defined(_WIN32)
        .def      \name
        .scl      2
        .type     32
        .endef
#else
        .hidden   \name
        .type     \name, @function
#endif
        .p2align  5
\name:
#if defined(_WIN32)
        .seh_proc \name
        // xmm6 to xmm15 are the caller's: their low halves are kept on the
        // stack.
        sub       rsp, 168
        .seh_stackalloc 168
        vmovaps   xmmword ptr [rsp], xmm6
        .seh_savexmm xmm6, 0
        vmovaps   xmmword ptr [rsp + 16], xmm7
        .seh_savexmm xmm7, 16
        vmovaps   xmmword ptr [rsp + 32], xmm8
        .seh_savexmm xmm8, 32
        vmovaps   xmmword ptr [rsp + 48], xmm9
        .seh_savexmm xmm9, 48
        vmovaps   xmmword ptr [rsp + 64], xmm10
        .seh_savexmm xmm10, 64
        vmovaps   xmmword ptr [rsp + 80], xmm11
        .seh_savexmm xmm11, 80
        vmovaps   xmmword ptr [rsp + 96], xmm12
        .seh_savexmm xmm12, 96
        vmovaps   xmmword ptr [rsp + 112], xmm13
        .seh_savexmm xmm13, 112
        vmovaps   xmmword ptr [rsp + 128], xmm14
        .seh_savexmm xmm14, 128
        vmovaps   xmmword ptr [rsp + 144], xmm15
        .seh_savexmm xmm15, 144
        .seh_endprologue
        // The core, what the pass goes by and the frame, in r8, r9 and r10.
        mov       r10, r8
        mov       r9, rdx
        mov       r8, rcx
#else
        mov       r10, rdx
        mov       r9, rsi
        mov       r8, rdi
#endif
        vpxor     xmm15, xmm15, xmm15
        SHARED_READ 0, 7, 10, IN_HOLD_13
        SHARED_READ 1, 8, 11, IN_HOLD_14
        cmp       dword ptr [r9 + IN_EG_SECOND], 0
        jne       .Lsecond\vnni
        PASS      0, \vnni
        jmp       .Lsums\vnni
.Lsecond\vnni:
        PASS      1, \vnni
.Lsums\vnni:
        HISTORY   0
        HISTORY   32
        HISTORY   64
        HISTORY   96
        HISTORY   128
        HISTORY   160
        SHARED_STORE 0, 7, 10, IN_HOLD_13
        SHARED_STORE 1, 8, 11, IN_HOLD_14
        vpxor     xmm8, xmm8, xmm8
        vpxor     xmm9, xmm9, xmm9
        vpxor     xmm10, xmm10, xmm10
        vpxor     xmm11, xmm11, xmm11
        cmp       dword ptr [r9 + IN_SOFT_PAN], 0
        jne       .Lsoft\vnni
        // Core::accumulate() without soft panning.
        HEARD     0, \vnni
        HEARD     32, \vnni
        HEARD     64, \vnni
        HEARD     96, \vnni
        HEARD     128, \vnni
        HEARD     160, \vnni
        vphaddd   ymm8, ymm8, ymm9
        vphaddd   ymm10, ymm10, ymm11
        vphaddd   ymm8, ymm8, ymm10
        vextracti128 xmm9, ymm8, 1
        vpaddd    xmm8, xmm8, xmm9                                    // a now, a late, b now, b late
        vmovq     xmm1, qword ptr [r8 + CARRY]
        vpshufd   xmm2, xmm8, 0x08                                    // a now, b now
        vpaddd    xmm2, xmm2, xmm1
        vpslld    xmm2, xmm2, 13
        vpsrad    xmm2, xmm2, 13
        vpminsd   xmm2, xmm2, xmmword ptr [rip + .Lc_32767]
        vpmaxsd   xmm2, xmm2, xmmword ptr [rip + .Lc_minus_32768]
        vmovq     qword ptr [r10], xmm2
        vpshufd   xmm3, xmm8, 0x0d                                    // a late, b late
        vmovq     qword ptr [r8 + CARRY], xmm3
        vpmovsxdq xmm4, xmm3
        vpsllq    xmm4, xmm4, 16
        vmovdqu   xmmword ptr [r8 + CARRY_SOFT], xmm4
        jmp       .Ldone\vnni
.Lsoft\vnni:
        // With soft panning: the sums exact in 64 bits, and one rounding at
        // the end.
        vpxor     xmm12, xmm12, xmm12
        vpxor     xmm13, xmm13, xmm13
        vpxor     xmm14, xmm14, xmm14
        vpxor     xmm15, xmm15, xmm15
        HEARD_SOFT 0, \vnni
        HEARD_SOFT 32, \vnni
        HEARD_SOFT 64, \vnni
        HEARD_SOFT 96, \vnni
        HEARD_SOFT 128, \vnni
        HEARD_SOFT 160, \vnni
        vphaddd   ymm12, ymm12, ymm13
        vphaddd   ymm14, ymm14, ymm15
        vphaddd   ymm12, ymm12, ymm14
        vextracti128 xmm13, ymm12, 1
        vpaddd    xmm12, xmm12, xmm13
        vpmovsxdq ymm12, xmm12
        vpsllq    ymm12, ymm12, 15                                    // the high parts' sums
        SUM64     xmm8, ymm8, xmm0, ymm1
        SUM64     xmm9, ymm9, xmm0, ymm1
        SUM64     xmm10, ymm10, xmm0, ymm1
        SUM64     xmm11, ymm11, xmm0, ymm1                            // the low parts' sums
        vmovq     rax, xmm12
        vmovq     rcx, xmm8
        add       rax, rcx                                            // a now
        vpextrq   r11, xmm12, 1
        vmovq     rcx, xmm9
        add       r11, rcx                                            // a late
        add       rax, qword ptr [r8 + CARRY_SOFT]
        mov       qword ptr [r8 + CARRY_SOFT], r11
        ROUND16   rax, rcx
        CLIP      eax, ecx
        mov       dword ptr [r10], eax
        ROUND16   r11, rcx
        mov       dword ptr [r8 + CARRY], r11d
        vextracti128 xmm12, ymm12, 1
        vmovq     rax, xmm12
        vmovq     rcx, xmm10
        add       rax, rcx                                            // b now
        vpextrq   r11, xmm12, 1
        vmovq     rcx, xmm11
        add       r11, rcx                                            // b late
        add       rax, qword ptr [r8 + CARRY_SOFT + 8]
        mov       qword ptr [r8 + CARRY_SOFT + 8], r11
        ROUND16   rax, rcx
        CLIP      eax, ecx
        mov       dword ptr [r10 + 4], eax
        ROUND16   r11, rcx
        mov       dword ptr [r8 + CARRY + 4], r11d
.Ldone\vnni:
        vzeroupper
#if defined(_WIN32)
        vmovaps   xmm6, xmmword ptr [rsp]
        vmovaps   xmm7, xmmword ptr [rsp + 16]
        vmovaps   xmm8, xmmword ptr [rsp + 32]
        vmovaps   xmm9, xmmword ptr [rsp + 48]
        vmovaps   xmm10, xmmword ptr [rsp + 64]
        vmovaps   xmm11, xmmword ptr [rsp + 80]
        vmovaps   xmm12, xmmword ptr [rsp + 96]
        vmovaps   xmm13, xmmword ptr [rsp + 112]
        vmovaps   xmm14, xmmword ptr [rsp + 128]
        vmovaps   xmm15, xmmword ptr [rsp + 144]
        add       rsp, 168
        ret
        .seh_endproc
#else
        ret
        .size     \name, . - \name
#endif
.endm

        PASS_FUNCTION adlplug_opl3_pass_avx2, 0
        PASS_FUNCTION adlplug_opl3_pass_avx2_vnni, 1

#if !defined(_WIN32)
        .section  .note.GNU-stack, "", @progbits
#endif
