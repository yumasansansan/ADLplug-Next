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
// (core.cc) written by hand for AVX-512, and what it works out for a slot is
// what ymf262_logic.h works out: the envelope, the phase, the operator, the
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
//     void adlplug_opl3_pass_avx512(Core *core, const Pass_inputs *inputs,
//                                   std::int32_t *frame);
//     void adlplug_opl3_pass_avx512_vnni(...);
//     void adlplug_opl3_pass_avx512_vbmi(...);
//     void adlplug_opl3_pass_avx512_vbmi_vnni(...);
//
// Each does what Core::generate() does between taking its writes in and moving
// its counters on (pass.h), as pass_avx2.S does, with AVX-512's foundation, its
// 16-bit and 8-bit lanes, its lengths of vector and its doublewords and
// quadwords (F, BW, VL, DQ), which the builds for AVX-512 ask for and the
// builds for AVX2 ask the processor for (core.cc). Those named with vnni add up
// the outputs with AVX-512 VNNI's multiplications that add into the sums
// (vpdpwssd), which core.cc takes on every processor with it. Those named with
// vbmi look their tables up by VBMI's permutations of bytes (vpermi2b), which
// it takes on Intel's processors alone (below). Intel has made processors with
// each of the two without the other: VNNI without VBMI (Cascade Lake, Cooper
// Lake), and VBMI without VNNI (Cannon Lake).
//
// The envelopes are worked out in 16 bits, thirty-two lanes to a register, and
// the rest in 32 bits, sixteen. The conditions are mask registers, the choices
// masked moves, and the envelope's shifts by a count for each lane are
// AVX-512's own (vpsllvw). The stages of operators wait for each other through
// two table lookups each, and here the two tables, the log-sine and the
// exponent, 256 entries of 16 bits each, are held in registers, zmm16 to zmm23
// and zmm24 to zmm31, and looked up by permutations between them. Without VBMI
// they are held in words and looked up by permutations of words (vpermi2w):
// four of 64 entries, two of the lookup's eight bits picking among them. With
// VBMI each is held as its low bytes and its high bytes, and looked up by
// permutations of bytes with the index as it is: two of 128 entries for each
// byte, the index's bit 7 picking, and the high byte moves up to join the low
// one. A lane of 32 bits looks up its low word, and the permutations leave its
// high word nought. On GitHub's runners a lookup by bytes waits 17.4 to 18.1
// cycles against 20.0 to 21.7 by words on Intel's processors (Ice Lake,
// Emerald Rapids, Granite Rapids), and the pass takes 5 to 6% less time on Ice
// Lake, 1 to 2% less on Granite Rapids and as long on Emerald Rapids; on AMD's
// (Zen 4, Zen 5) it waits 16.3 to 17.2 cycles against 13.9 to 14.5, and the
// pass takes 2 to 4% more time.
//
// A load of what was stored just before takes it from the store, if it takes
// nothing that another store left: a store, a load of what it stored and an
// addition take 8 to 9 cycles on Intel's processors, 12 to 13 on AMD's, but 21
// to 23 and 16 to 17 when the load spans two stores and so waits for both to
// reach the cache. What a stage stored is loaded as it was stored, its lanes
// chosen after, and the lanes that the stages of eight stored, 32 to 47, are
// loaded eight at a time for the history and the sums, which takes 2 to 4% off
// the time of the pass.
//
// Assembled by Clang, in Intel's syntax, for ELF (Linux) and COFF (Windows).

#include "pass.h"

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
        .set IN_LOGSIN16, ADLPLUG_OPL3_IN_LOGSIN16
        .set IN_EXP16, ADLPLUG_OPL3_IN_EXP16
        .set IN_LOGSIN8, ADLPLUG_OPL3_IN_LOGSIN8
        .set IN_EXP8, ADLPLUG_OPL3_IN_EXP8

#if defined(_WIN32)
        .section .rdata, "dr"
#else
        .section .rodata
#endif
        .p2align 6
// Vectors of sixteen 32-bit lanes, of which the first eight serve a ymm.
.Lc_1:          .rept 16
                .long 1
                .endr
.Lc_2:          .rept 16
                .long 2
                .endr
.Lc_255:        .rept 16
                .long 255
                .endr
.Lc_511:        .rept 16
                .long 511
                .endr
.Lc_1023:       .rept 16
                .long 1023
                .endr
.Lc_0x7ffff:    .rept 16
                .long 0x7ffff
                .endr
.Lc_0x7fff:     .rept 16
                .long 0x7fff
                .endr
// Vectors of thirty-two 16-bit lanes.
.Lw_1:          .rept 32
                .short 1
                .endr
.Lw_3:          .rept 32
                .short 3
                .endr
.Lw_4:          .rept 32
                .short 4
                .endr
.Lw_12:         .rept 32
                .short 12
                .endr
.Lw_minus_12:   .rept 32
                .short -12
                .endr
.Lw_15:         .rept 32
                .short 15
                .endr
.Lw_16:         .rept 32
                .short 16
                .endr
.Lw_64:         .rept 32
                .short 64
                .endr
.Lw_128:        .rept 32
                .short 128
                .endr
.Lw_255:        .rept 32
                .short 255
                .endr
.Lw_0x1f7:      .rept 32
                .short 0x1f7
                .endr
.Lw_0x1ff:      .rept 32
                .short 0x1ff
                .endr
.Lw_0x200:      .rept 32
                .short 0x200
                .endr
// vpshufb's tables, one to each 128-bit part of a vector, and the bytes that
// put a lane's index into them: the index in the low byte, and in the high byte
// an index of its own.
//
// By the sum of a rate and the timer's step (0 to 15): the key scalings, a bit
// for each value of their low two bits, for which the low step falls.
.Lb_low_step:   .rept 4
                .byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x0f, 0x0c, 0x0a, 0
                .endr
// By a step (0 to 3): the low byte of the attack's high bits, and at 4 their
// 0x100, which the index's high byte takes.
.Lb_attack_bits: .rept 4
                .byte 0x40, 0xc0, 0x80, 0x00, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
                .endr
.Lw_attack_bits_index: .rept 32
                .short 0x0400
                .endr
// Vectors of four 32-bit lanes, for the sums at the end.
.Lc_32767:      .rept 4
                .long 32767
                .endr
.Lc_minus_32768: .rept 4
                .long -32768
                .endr

        .text

// The 32-bit values of the lanes at byte \g, two vectors of \w (z or y) apart
// by \half bytes, in 16 bits into \w\()mm\dst, in the order vpackusdw leaves
// them in: in each 128-bit part, four lanes of the first vector, then four of
// the second. The envelopes keep their levels and states in that order too.
.macro PACK w, dst, offset, g, half
        vmovdqu32 \w\()mm\dst, \w\()mmword ptr [r8 + \offset + \g]
        vpackusdw \w\()mm\dst, \w\()mm\dst, \w\()mmword ptr [r8 + \offset + \g + \half]
.endm

// The envelopes of the lanes at byte \g (ymf262::envelope() and attenuation()),
// thirty-two of them in zmm (\w z, \half 64) or sixteen in ymm (\w y, \half 32):
// the state and the level for the next pass, and what the operator takes of
// this pass's level. The rate a state goes by is taken from its byte in 32
// bits. Leaves dokon (keyed on from release) in 32 bits for the phases: the
// first vector's lanes in mm9, the second's in mm10. \second is the envelope
// timer's second half. zmm15 holds nought.
.macro ENVELOPE w, second, g, half
        vmovdqu16 \w\()mm0, \w\()mmword ptr [r8 + STATE16 + \g / 2]
        vmovdqu16 \w\()mm1, \w\()mmword ptr [r8 + LEVEL16 + \g / 2]
        PACK      \w, 2, KEY_ON, \g, \half
        vptestmw  k1, \w\()mm2, \w\()mm2                                  // keyed
        vpcmpeqw  k2 {k1}, \w\()mm0, \w\()mmword ptr [rip + .Lw_3]       // dokon
        vpmovm2w  \w\()mm3, k2
        vpblendmw \w\()mm4 {k2}, \w\()mm0, \w\()mm15                     // the state the rate goes by
        vpunpcklwd \w\()mm5, \w\()mm4, \w\()mm15
        vpunpckhwd \w\()mm4, \w\()mm4, \w\()mm15
        vpslld    \w\()mm5, \w\()mm5, 3
        vpslld    \w\()mm4, \w\()mm4, 3
        vmovdqu32 \w\()mm6, \w\()mmword ptr [r8 + RATES + \g]
        vpsrlvd   \w\()mm5, \w\()mm6, \w\()mm5
        vmovdqu32 \w\()mm6, \w\()mmword ptr [r8 + RATES + \g + \half]
        vpsrlvd   \w\()mm4, \w\()mm6, \w\()mm4
        vpandd    \w\()mm5, \w\()mm5, \w\()mmword ptr [rip + .Lc_255]
        vpandd    \w\()mm4, \w\()mm4, \w\()mmword ptr [rip + .Lc_255]
        vpackusdw \w\()mm4, \w\()mm5, \w\()mm4                           // its rate
        vpandd    \w\()mm5, \w\()mm4, \w\()mmword ptr [rip + .Lw_15]     // rate_hi
        PACK      \w, 6, KEY_SCALE_LOW, \g, \half
        vpbroadcastw \w\()mm7, word ptr [r9 + IN_HIGH_FROM_LESS_1]
        vpcmpgtw  k3, \w\()mm6, \w\()mm7                                  // stephi
.if \second
        vpcmpeqw  k4, \w\()mm5, \w\()mmword ptr [rip + .Lw_12]
        kord      k3, k3, k4                                              // high
.endif
        vpaddw    \w\()mm8, \w\()mm5, \w\()mmword ptr [rip + .Lw_minus_12]
        vpaddw    \w\()mm8 {k3}, \w\()mm8, \w\()mmword ptr [rip + .Lw_1]
        vpminsw   \w\()mm8, \w\()mm8, \w\()mmword ptr [rip + .Lw_3]      // the step from rate 12, none below
        vpmaxsw   \w\()mm8, \w\()mm8, \w\()mm15
.if \second
        // The low step, below rate 12. Rates of 12 and up take a step of 1 at
        // least when the timer's second half is on, which the low step cannot
        // raise, so it is not asked whether the rate is below 12.
        vpbroadcastw \w\()mm7, word ptr [r9 + IN_ADD]
        vpaddw    \w\()mm7, \w\()mm5, \w\()mm7
        vpandd    \w\()mm7, \w\()mm7, \w\()mmword ptr [rip + .Lw_15]     // sum
        vmovdqa64 \w\()mm9, \w\()mmword ptr [rip + .Lb_low_step]
        vpshufb   \w\()mm7, \w\()mm9, \w\()mm7
        vmovdqa64 \w\()mm9, \w\()mmword ptr [rip + .Lw_1]
        vpsllvw   \w\()mm6, \w\()mm9, \w\()mm6                           // this key scaling's bit
        vptestmw  k4, \w\()mm4, \w\()mmword ptr [rip + .Lw_16]           // the register's rate is not 0
        vptestmw  k4 {k4}, \w\()mm7, \w\()mm6                             // inclow
        vpmaxsw   \w\()mm8 {k4}, \w\()mm8, \w\()mm9                       // step
.endif
        vptestnmw k3, \w\()mm0, \w\()mm0                                  // attack
        vptestnmw k4, \w\()mm1, \w\()mm1                                  // zeroreach
        PACK      \w, 6, SUSTAIN, \g, \half
        vpsrlw    \w\()mm7, \w\()mm1, 4
        vpcmpeqw  k7, \w\()mm6, \w\()mm7                                  // slreach
        vpcmpeqw  k5 {k7}, \w\()mm0, \w\()mmword ptr [rip + .Lw_1]       // decay at the sustain level
        kandd     k6, k3, k4
        kord      k7, k5, k6                                              // one state on
        // The state for the next pass: one on from attack at level 0 and from
        // decay at the sustain level, release with the key off, attack on dokon.
        vmovdqa64 \w\()mm12, \w\()mm0
        vpaddw    \w\()mm12 {k7}, \w\()mm12, \w\()mmword ptr [rip + .Lw_1]
        knotd     k6, k1
        vmovdqu16 \w\()mm12 {k6}, \w\()mmword ptr [rip + .Lw_3]
        vmovdqu16 \w\()mm12 {k2}, \w\()mm15
        vmovdqu16 \w\()mmword ptr [r8 + STATE16 + \g / 2], \w\()mm12
        vpcmpeqw  k6, \w\()mm5, \w\()mmword ptr [rip + .Lw_15]           // maxrate
        kord      k7, k6, k4
        kandd     k4, k3, k1
        kandnd    k7, k7, k4                                              // exponent
        kandd     k6, k6, k2                                              // instant
        vpcmpgtw  k1, \w\()mm1, \w\()mmword ptr [rip + .Lw_0x1f7]        // silent
        kord      k4, k3, k2
        kandnd    k4, k4, k1                                              // mute
        // Linear is attack, decay at the sustain level, dokon and silence all
        // wanting: the state is decay short of the sustain level, sustain or
        // release.
        kord      k3, k3, k5
        kord      k3, k3, k2
        kord      k3, k3, k1
        knotd     k3, k3                                                  // linear
        vmovdqa64 \w\()mm14, \w\()mm1
        vmovdqu16 \w\()mm14 {k6}, \w\()mm15
        vmovdqu16 \w\()mm14 {k4}, \w\()mmword ptr [rip + .Lw_0x1ff]      // the level before the step
        vpsrlw    \w\()mm4, \w\()mm1, 1
        vpxord    \w\()mm4, \w\()mm4, \w\()mmword ptr [rip + .Lw_255]
        vmovdqu16 \w\()mm4 {k7}{z}, \w\()mm4
        vmovdqu16 \w\()mm4 {k3}, \w\()mmword ptr [rip + .Lw_4]           // the increment
        vpord     \w\()mm12, \w\()mm8, \w\()mmword ptr [rip + .Lw_attack_bits_index]
        vmovdqa64 \w\()mm10, \w\()mmword ptr [rip + .Lb_attack_bits]
        vpshufb   \w\()mm12, \w\()mm10, \w\()mm12
        vmovdqu16 \w\()mm12 {k7}{z}, \w\()mm12                            // the attack's high bits
        // The increment shifted down by 3 less the step.
        vpsllvw   \w\()mm4, \w\()mm4, \w\()mm8
        vpsrlw    \w\()mm4, \w\()mm4, 3
        vpord     \w\()mm4, \w\()mm4, \w\()mm12
        vptestmw  k1, \w\()mm8, \w\()mm8                                  // a step, else nothing
        vpaddw    \w\()mm14 {k1}, \w\()mm14, \w\()mm4
        vpandd    \w\()mm14, \w\()mm14, \w\()mmword ptr [rip + .Lw_0x1ff]
        vmovdqu16 \w\()mmword ptr [r8 + LEVEL16 + \g / 2], \w\()mm14
        // The attenuation the operator takes: this pass's level with the key
        // scale level, the total level and the tremolo, all ones past nine bits.
        PACK      \w, 5, TREMOLO_DEPTH, \g, \half
        vbroadcasti32x4 \w\()mm6, xmmword ptr [r9 + IN_TREMOLO]
        vpshufb   \w\()mm5, \w\()mm6, \w\()mm5                            // the tremolo
        PACK      \w, 6, LEVEL_OFFSET, \g, \half
        vpaddw    \w\()mm5, \w\()mm5, \w\()mm6
        vpandd    \w\()mm6, \w\()mm5, \w\()mmword ptr [rip + .Lw_0x1ff]
        vpaddw    \w\()mm6, \w\()mm6, \w\()mm1
        vpandd    \w\()mm5, \w\()mm5, \w\()mmword ptr [rip + .Lw_0x200]
        vpord     \w\()mm6, \w\()mm6, \w\()mm5
        vpminsw   \w\()mm6, \w\()mm6, \w\()mmword ptr [rip + .Lw_0x1ff]
        vpunpcklwd \w\()mm5, \w\()mm6, \w\()mm15
        vmovdqu32 \w\()mmword ptr [r8 + EG_OUT + \g], \w\()mm5
        vpunpckhwd \w\()mm6, \w\()mm6, \w\()mm15
        vmovdqu32 \w\()mmword ptr [r8 + EG_OUT + \g + \half], \w\()mm6
        vpunpcklwd \w\()mm9, \w\()mm3, \w\()mm3
        vpunpckhwd \w\()mm10, \w\()mm3, \w\()mm3
.endm

// The phases of the sixteen lanes at byte \v (ymf262::phase_increment(),
// phase_out() and next_phase()): what the operator reads of this pass's phase,
// and the phase for the next pass, from nought on dokon (\restart).
.macro PHASE v, restart
        vmovdqu32 zmm4, zmmword ptr [r8 + VIBRATO_DEPTH + \v]
        vmovdqu32 zmm5, zmmword ptr [r8 + FNUM + \v]
        vpsllvd   zmm6, zmm5, zmm4
        vpsrlvd   zmm6, zmm6, dword ptr [r9 + IN_VIBRATO_SHIFT] {1to16}  // fnum shifted down as the depth asks
        vpxord    zmm4, zmm4, dword ptr [rip + .Lc_1] {1to16}
        vpbroadcastd zmm7, dword ptr [r9 + IN_VIBRATO_MASK]
        vpsrlvd   zmm4, zmm7, zmm4                                        // the depth's bits
        vpandd    zmm6, zmm6, zmm4
        vmovdqu32 zmm4, zmmword ptr [r8 + VIBRATO + \v]
        vptestmd  k1, zmm4, zmm4
        vmovdqa32 zmm6 {k1}{z}, zmm6                                       // the vibrato's swing
        vpbroadcastd zmm7, dword ptr [r9 + IN_VIBRATO_SIGN]
        vpsrad    zmm7, zmm7, 1                                            // all ones when it takes off
        vpxord    zmm6, zmm6, zmm7
        vpsubd    zmm6, zmm6, zmm7
        vpaddd    zmm5, zmm5, zmm6
        vpandd    zmm5, zmm5, dword ptr [r9 + IN_VIBRATO_WRAP] {1to16}    // the frequency number
        vpsllvd   zmm5, zmm5, zmmword ptr [r8 + BLOCK + \v]
        vpsrld    zmm5, zmm5, 1
        vpmulld   zmm5, zmm5, zmmword ptr [r8 + MULTIPLE + \v]
        vpsrld    zmm5, zmm5, 1                                            // the increment
        vmovdqu32 zmm6, zmmword ptr [r8 + PHASE + \v]
        vpsrld    zmm7, zmm6, 9
        vmovdqu32 zmmword ptr [r8 + PHASE_OUT + \v], zmm7
        vpandnd   zmm6, \restart, zmm6
        vpaddd    zmm6, zmm6, zmm5
        vpandd    zmm6, zmm6, dword ptr [rip + .Lc_0x7ffff] {1to16}
        vmovdqu32 zmmword ptr [r8 + PHASE + \v], zmm6
.endm

// The low word of each 32-bit lane of \idx, below 256, looked up into \dst in
// the table held in \r0 to \r7, eight registers of 32 words: a lookup of 64
// entries in each pair, bits 6 and 7 of the index picking among the four, with
// \a, \b and \c and the mask registers \m6 and \m7. The permutations leave the
// high words nought, by the mask of the low words in k7 (OPERATOR).
.macro LOOKUP dst, idx, a, b, c, m6, m7, r0, r1, r2, r3, r4, r5, r6, r7
        vmovdqa64 \dst, \idx
        vpermi2w  \dst {k7}{z}, \r0, \r1
        vmovdqa64 \a, \idx
        vpermi2w  \a {k7}{z}, \r2, \r3
        vmovdqa64 \b, \idx
        vpermi2w  \b {k7}{z}, \r4, \r5
        vmovdqa64 \c, \idx
        vpermi2w  \c {k7}{z}, \r6, \r7
        vptestmw  \m6, \idx, zmmword ptr [rip + .Lw_64]
        vptestmw  \m7, \idx, zmmword ptr [rip + .Lw_128]
        vpblendmw \dst {\m6}, \dst, \a
        vpblendmw \b {\m6}, \b, \c
        vpblendmw \dst {\m7}, \dst, \b
.endm

// The same by VBMI's permutations of bytes, on the table held as its low bytes
// in \r0 to \r3 and its high bytes in \r4 to \r7, with the index as it is: its
// low seven bits pick a byte of a pair of registers and its bit 7 the pair
// (\m). Only each lane's low byte is looked up, by the mask in k7 (OPERATOR),
// and the high byte moves up eight bits to join the low one.
.macro LOOKUP_VBMI dst, idx, a, b, c, m, r0, r1, r2, r3, r4, r5, r6, r7
        vmovdqa64 \dst, \idx
        vpermi2b  \dst {k7}{z}, \r0, \r1
        vmovdqa64 \a, \idx
        vpermi2b  \a {k7}{z}, \r2, \r3
        vmovdqa64 \b, \idx
        vpermi2b  \b {k7}{z}, \r4, \r5
        vmovdqa64 \c, \idx
        vpermi2b  \c {k7}{z}, \r6, \r7
        vpmovb2m  \m, \idx
        vpblendmb \dst {\m}, \dst, \a
        vpblendmb \b {\m}, \b, \c
        vpslld    \b, \b, 8
        vpord     \dst, \dst, \b
.endm

// The operators of the lanes at byte \v (ymf262::modulation() and
// operator_output()), sixteen in zmm (\w z) or eight in ymm (\w y), modulated,
// when they are by the stage before, by the lanes at byte \source; their tables
// looked up by VBMI with \vbmi 1.
.macro OPERATOR w, v, source, vbmi
        // The lookups' mask: each lane's low word for the permutations of
        // words, its low byte for those of bytes.
.if \vbmi
        mov       rax, 0x1111111111111111
        kmovq     k7, rax
.else
        mov       eax, 0x55555555
        kmovd     k7, eax
.endif
        // First what does not wait for the stage before: the feedback, added to
        // the phase as the modulation, to which the stage before's modulation is
        // added later (no slot has both).
        vmovdqu32 \w\()mm0, \w\()mmword ptr [r8 + MODULATED_BY + \v]
        vpcmpeqd  k1, \w\()mm0, \w\()mmword ptr [rip + .Lc_1]            // by the stage before
        vpcmpeqd  k2, \w\()mm0, \w\()mmword ptr [rip + .Lc_2]            // by feedback
        vmovdqu32 \w\()mm2, \w\()mmword ptr [r8 + OUT_1 + \v]
        vpaddd    \w\()mm2, \w\()mm2, \w\()mmword ptr [r8 + OUT_2 + \v]
        vpsravd   \w\()mm2 {k2}{z}, \w\()mm2, \w\()mmword ptr [r8 + FEEDBACK_SHIFT + \v]
        vpandd    \w\()mm2, \w\()mm2, \w\()mmword ptr [rip + .Lc_1023]   // the feedback
        vpaddd    \w\()mm2, \w\()mm2, \w\()mmword ptr [r8 + PHASE_OUT + \v]
        vmovdqu32 \w\()mm5, \w\()mmword ptr [r8 + TURN_BIT + \v]
        vmovdqu32 \w\()mm3, \w\()mmword ptr [r8 + SQUARE + \v]
        vptestmd  k3, \w\()mm3, \w\()mm3                                  // square
        vmovdqu32 \w\()mm3, \w\()mmword ptr [r8 + SAWTOOTH + \v]
        vptestmd  k6, \w\()mm3, \w\()mm3                                  // sawtooth
        // From the stage before to the log-sine's index. What the stage
        // before stored is loaded as it was stored, and its lanes chosen after.
        vmovdqu32 \w\()mm1, \w\()mmword ptr [r8 + OUT + \source]
        vpandd    \w\()mm1 {k1}{z}, \w\()mm1, \w\()mmword ptr [rip + .Lc_1023]
        vpaddd    \w\()mm1, \w\()mm1, \w\()mm2                            // the phase
        vpsllvd   \w\()mm2, \w\()mm1, \w\()mmword ptr [r8 + PHASE_SHIFT + \v] // shifted, with the bits above its ten
        vptestmd  k4, \w\()mm2, \w\()mm5                                  // turned back
        vmovdqa64 \w\()mm4, \w\()mm2
        vpternlogd \w\()mm4 {k4}, \w\()mm4, \w\()mm4, 0x0f                // phase2, in its low nine bits
        vpandd    \w\()mm5, \w\()mm4, \w\()mmword ptr [rip + .Lc_255]
        vmovdqu32 \w\()mm5 {k3}, \w\()mmword ptr [rip + .Lc_255]         // the log-sine's index
.if \vbmi
        LOOKUP_VBMI zmm7, zmm5, zmm9, zmm10, zmm11, k2, zmm16, zmm17, zmm18, zmm19, zmm20, zmm21, zmm22, zmm23
.else
        LOOKUP    zmm7, zmm5, zmm9, zmm10, zmm11, k2, k3, zmm16, zmm17, zmm18, zmm19, zmm20, zmm21, zmm22, zmm23
.endif
        // While it is looked up: the ramp, the sign (bit 9 of shifted, which
        // turning back leaves alone) and the mute.
        vpandd    \w\()mm4, \w\()mm4, \w\()mmword ptr [rip + .Lc_511]
        vpslld    \w\()mm4, \w\()mm4, 3                                   // the ramp, for the sawtooth
        vptestnmd k4, \w\()mm1, \w\()mmword ptr [r8 + MUTE_BIT + \v]      // heard
        vptestmd  k5, \w\()mm2, \w\()mmword ptr [r8 + SIGN_BIT + \v]
        vpmovm2d  \w\()mm2, k5                                            // negative
        vmovdqu32 \w\()mm0, \w\()mmword ptr [r8 + EG_OUT + \v]
        vpslld    \w\()mm0, \w\()mm0, 3
        // From the sine to the exponent's index. att is not held to 4095: past
        // it, it shifts the magnitude out all the same.
        vmovdqa32 \w\()mm7 {k6}, \w\()mm4                                 // shape
        vpaddd    \w\()mm7, \w\()mm7, \w\()mm0                            // att
        vpandd    \w\()mm3, \w\()mm7, \w\()mmword ptr [rip + .Lc_255]
.if \vbmi
        LOOKUP_VBMI zmm5, zmm3, zmm9, zmm10, zmm11, k2, zmm24, zmm25, zmm26, zmm27, zmm28, zmm29, zmm30, zmm31
.else
        LOOKUP    zmm5, zmm3, zmm9, zmm10, zmm11, k2, k3, zmm24, zmm25, zmm26, zmm27, zmm28, zmm29, zmm30, zmm31
.endif
        vpsrld    \w\()mm7, \w\()mm7, 8
        vpsrlvd   \w\()mm5, \w\()mm5, \w\()mm7                            // magnitude, below 4096
        // The value: nought when muted, and when negative the magnitude's
        // complement, which is the 13-bit ones' complement taken as signed.
        vpxord    \w\()mm5 {k4}{z}, \w\()mm5, \w\()mm2
        vmovdqu32 \w\()mmword ptr [r8 + OUT + \v], \w\()mm5
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

// This pass's values of the sixteen lanes at byte \v into zmm0: those of lanes
// 32 to 47 (\v 128) eight at a time, as the two stages of eight stored them.
.macro LOAD_OUT v
.if \v == 128
        vmovdqu32 ymm0, ymmword ptr [r8 + OUT + \v]
        vinserti64x4 zmm0, zmm0, ymmword ptr [r8 + OUT + \v + 32], 1
.else
        vmovdqu32 zmm0, zmmword ptr [r8 + OUT + \v]
.endif
.endm

// The last two values of the sixteen lanes at byte \v.
.macro HISTORY v
        vmovdqu32 zmm0, zmmword ptr [r8 + OUT_1 + \v]
        vmovdqu32 zmmword ptr [r8 + OUT_2 + \v], zmm0
        LOAD_OUT  \v
        vmovdqu32 zmmword ptr [r8 + OUT_1 + \v], zmm0
.endm

// The products of the values of the sixteen lanes at byte \v and their weights
// on the outputs, added into zmm8 to zmm11. A value fits 16 bits and a weight is
// 2 at the most, so each lane's product is that of its low 16 bits. With \vnni,
// AVX-512 VNNI's multiplications that add into the sums; without,
// multiplications that add pairs, then additions.
.macro HEARD v, vnni
        LOAD_OUT  \v
.if \vnni
        vpdpwssd  zmm8, zmm0, zmmword ptr [r8 + A_NOW + \v]
        vpdpwssd  zmm9, zmm0, zmmword ptr [r8 + A_LATE + \v]
        vpdpwssd  zmm10, zmm0, zmmword ptr [r8 + B_NOW + \v]
        vpdpwssd  zmm11, zmm0, zmmword ptr [r8 + B_LATE + \v]
.else
        vpmaddwd  zmm1, zmm0, zmmword ptr [r8 + A_NOW + \v]
        vpmaddwd  zmm2, zmm0, zmmword ptr [r8 + A_LATE + \v]
        vpmaddwd  zmm3, zmm0, zmmword ptr [r8 + B_NOW + \v]
        vpmaddwd  zmm4, zmm0, zmmword ptr [r8 + B_LATE + \v]
        vpaddd    zmm8, zmm8, zmm1
        vpaddd    zmm9, zmm9, zmm2
        vpaddd    zmm10, zmm10, zmm3
        vpaddd    zmm11, zmm11, zmm4
.endif
.endm

// The same with soft panning, whose weights reach 2^17: each as its low fifteen
// bits and the rest, the products of the former added into zmm8 to zmm11, of the
// latter into zmm12 to zmm15. Folded in two (FOLD), a lane holds the sum of six
// products of the former, within 2^30.
.macro HEARD_SOFT v, vnni
        LOAD_OUT  \v
        HEARD_SOFT_ONE \v, SOFT_A_NOW, zmm8, zmm12, \vnni
        HEARD_SOFT_ONE \v, SOFT_A_LATE, zmm9, zmm13, \vnni
        HEARD_SOFT_ONE \v, SOFT_B_NOW, zmm10, zmm14, \vnni
        HEARD_SOFT_ONE \v, SOFT_B_LATE, zmm11, zmm15, \vnni
.endm

.macro HEARD_SOFT_ONE v, offset, low, high, vnni
        vmovdqu32 zmm1, zmmword ptr [r8 + \offset + \v]
        vpandd    zmm2, zmm1, dword ptr [rip + .Lc_0x7fff] {1to16}
        vpsrld    zmm1, zmm1, 15
.if \vnni
        vpdpwssd  \low, zmm0, zmm2
        vpdpwssd  \high, zmm0, zmm1
.else
        vpmaddwd  zmm2, zmm0, zmm2
        vpmaddwd  zmm1, zmm0, zmm1
        vpaddd    \low, \low, zmm2
        vpaddd    \high, \high, zmm1
.endif
.endm

// The sixteen lanes of zmm\r folded into the eight of ymm\r, with ymm\t.
.macro FOLD r, t
        vextracti64x4 ymm\t, zmm\r, 1
        vpaddd    ymm\r, ymm\r, ymm\t
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
// timer's second half off (\second 0) or on (1), looking the tables up by VBMI
// with \vbmi 1. Stages 0 and 1, lanes 0 to 31, have their envelopes and phases
// first; stage 0 is lanes 0 to 15, stage 1 lanes 16 to 31, modulated by stage 0.
// Stages 2 (lanes 32 to 39) and 3 (lanes 40 to 47), modulated by stages 1 and
// 2, have theirs between stages 0 and 1. The hi-hat's phase comes before stage
// 0, and the snare's and the top cymbal's before stage 1.
.macro PASS second, vbmi
        ENVELOPE  z, \second, 0, 64
        PHASE     0, zmm9
        PHASE     64, zmm10
        HI_HAT
        OPERATOR  z, 0, 0, \vbmi
        ENVELOPE  y, \second, 128, 32
        vinserti64x4 zmm9, zmm9, ymm10, 1
        PHASE     128, zmm9
        SNARE_AND_TOP_CYMBAL
        OPERATOR  z, 64, 0, \vbmi
        OPERATOR  y, 128, 64, \vbmi
        OPERATOR  y, 160, 128, \vbmi
.endm

// The pass \name, adding up with AVX-512 VNNI with \vnni 1 and looking the
// tables up by VBMI with \vbmi 1. Its labels end in \vnni\vbmi.
.macro PASS_FUNCTION name, vnni, vbmi
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
        vpxord    xmm15, xmm15, xmm15
        // The tables of the log-sine and of the exponent: in words, or with
        // VBMI as their low bytes, then their high bytes.
.if \vbmi
        mov       rax, qword ptr [r9 + IN_LOGSIN8]
.else
        mov       rax, qword ptr [r9 + IN_LOGSIN16]
.endif
        vmovdqu16 zmm16, zmmword ptr [rax]
        vmovdqu16 zmm17, zmmword ptr [rax + 64]
        vmovdqu16 zmm18, zmmword ptr [rax + 128]
        vmovdqu16 zmm19, zmmword ptr [rax + 192]
        vmovdqu16 zmm20, zmmword ptr [rax + 256]
        vmovdqu16 zmm21, zmmword ptr [rax + 320]
        vmovdqu16 zmm22, zmmword ptr [rax + 384]
        vmovdqu16 zmm23, zmmword ptr [rax + 448]
.if \vbmi
        mov       rax, qword ptr [r9 + IN_EXP8]
.else
        mov       rax, qword ptr [r9 + IN_EXP16]
.endif
        vmovdqu16 zmm24, zmmword ptr [rax]
        vmovdqu16 zmm25, zmmword ptr [rax + 64]
        vmovdqu16 zmm26, zmmword ptr [rax + 128]
        vmovdqu16 zmm27, zmmword ptr [rax + 192]
        vmovdqu16 zmm28, zmmword ptr [rax + 256]
        vmovdqu16 zmm29, zmmword ptr [rax + 320]
        vmovdqu16 zmm30, zmmword ptr [rax + 384]
        vmovdqu16 zmm31, zmmword ptr [rax + 448]
        SHARED_READ 0, 7, 10, IN_HOLD_13
        SHARED_READ 1, 8, 11, IN_HOLD_14
        cmp       dword ptr [r9 + IN_EG_SECOND], 0
        jne       .Lsecond\vnni\vbmi
        PASS      0, \vbmi
        jmp       .Lsums\vnni\vbmi
.Lsecond\vnni\vbmi:
        PASS      1, \vbmi
.Lsums\vnni\vbmi:
        HISTORY   0
        HISTORY   64
        HISTORY   128
        SHARED_STORE 0, 7, 10, IN_HOLD_13
        SHARED_STORE 1, 8, 11, IN_HOLD_14
        vpxord    xmm8, xmm8, xmm8
        vpxord    xmm9, xmm9, xmm9
        vpxord    xmm10, xmm10, xmm10
        vpxord    xmm11, xmm11, xmm11
        cmp       dword ptr [r9 + IN_SOFT_PAN], 0
        jne       .Lsoft\vnni\vbmi
        // Core::accumulate() without soft panning.
        HEARD     0, \vnni
        HEARD     64, \vnni
        HEARD     128, \vnni
        FOLD      8, 1
        FOLD      9, 1
        FOLD      10, 1
        FOLD      11, 1
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
        jmp       .Ldone\vnni\vbmi
.Lsoft\vnni\vbmi:
        // With soft panning: the sums exact in 64 bits, and one rounding at
        // the end.
        vpxord    xmm12, xmm12, xmm12
        vpxord    xmm13, xmm13, xmm13
        vpxord    xmm14, xmm14, xmm14
        vpxord    xmm15, xmm15, xmm15
        HEARD_SOFT 0, \vnni
        HEARD_SOFT 64, \vnni
        HEARD_SOFT 128, \vnni
        FOLD      8, 1
        FOLD      9, 1
        FOLD      10, 1
        FOLD      11, 1
        FOLD      12, 1
        FOLD      13, 1
        FOLD      14, 1
        FOLD      15, 1
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
.Ldone\vnni\vbmi:
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

        PASS_FUNCTION adlplug_opl3_pass_avx512, 0, 0
        PASS_FUNCTION adlplug_opl3_pass_avx512_vnni, 1, 0
        PASS_FUNCTION adlplug_opl3_pass_avx512_vbmi, 0, 1
        PASS_FUNCTION adlplug_opl3_pass_avx512_vbmi_vnni, 1, 1

#if !defined(_WIN32)
        .section  .note.GNU-stack, "", @progbits
#endif
