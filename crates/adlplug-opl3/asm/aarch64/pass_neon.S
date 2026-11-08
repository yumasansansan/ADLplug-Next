// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// It is a pass of ADLplug-Next OPL3's core (src/core.rs) written by hand for
// NEON, and what it works out for a slot is what src/ymf262.rs works out: the
// envelope, the phase, the operator, the modulation and the rhythm phases.
// src/ymf262.rs follows the code of YMF262-LLE by nukeykt, an emulator of the
// YMF262 (OPL3) drawn from a die shot of the chip, and carries its notice; this
// file holds none of that code, and works the same out for the lanes of the
// core in instructions of its own. The counters that src/ymf262.rs takes from
// Nuked OPL3 stay in src/core.rs, which hands this pass what they give.
//
//     void adlplug_opl3_pass_neon(Lanes *lanes, const PassInputs *inputs,
//                                 int32_t *frame);
//     void adlplug_opl3_pass_sve(Lanes *lanes, const PassInputs *inputs,
//                                int32_t *frame);
//         AAPCS64
//
// What Core::generate() does between taking its writes in and moving its
// counters on (src/aarch64.rs), for NEON (Advanced SIMD), which every AArch64
// processor has: the envelopes and the phases, the rhythm phases, the operators
// stage by stage, the feedback history and the sums of the two outputs. What
// it works out for a slot is what src/ymf262.rs works out, in the order the
// pass for AVX2 works it out (x86_64/pass_avx2.S), each of its vectors of
// eight lanes two vectors of four here.
//
// The lanes are those of the core, four to a vector of 32 bits, but for the
// envelopes, which are worked out eight to a vector of 16 bits: every value
// they go by fits, and so twice as many lanes go through each instruction. The
// envelopes keep their levels and states in the lanes' own order, which the
// narrowing instructions leave them in. NEON shifts each lane by a count of its
// own, in 16 bits as in 32 (ushl, sshl, the count negative for a shift down),
// and looks a byte up in a table of sixteen (tbl, nought past the table), so
// the envelope's steps are shifts and its small tables tbl. The tables of the
// log-sine and the exponent are looked up one lane at a time, as the pass for
// AVX2 without AVX-VNNI looks them up: NEON has no gather.
//
// The second function, this branch's experiment, is the first with those two
// tables looked up by SVE's gathers (ld1w), four lanes at a time under a
// predicate of four elements (ptrue vl4), so that it works the same on SVE's
// vectors of any length: the rest of the pass is NEON's, and a NEON instruction
// that writes a vector zeroes the bits of its SVE register above it, which
// nothing reads. Both are assembled from the same macros, the second's lookups
// alone different.
//
// The stages of operators wait for each other, through two table lookups each,
// so each stage starts with what does not wait for the stage before, and the
// envelopes and phases of the next stage come between stages, for the
// processor to work on while it waits.
//
// Registers: x0 the lanes, x1 what the pass goes by, x2 the frame; x3 and x4
// the tables of the log-sine and of the exponent; x5 the lanes' values from
// OUT on, for the loads and stores in pairs; x9 to x15 for the scalars and the
// lookups. The vectors are v0 to v7 and v16 to v31, all of them the caller's
// to save, and in the form for SVE z3, z5 and z7 and the predicate p0, the
// caller's as well; v8 to v15, whose low halves the convention has the callee
// keep, are not touched, nor x18, nor the stack. So the function needs no frame and no
// unwinding information on any of the systems.
//
// Assembled by Clang, for Mach-O (macOS), ELF (Linux) and COFF (Windows).

#include "pass.h"

// The form for SVE takes its instructions, which the assembler is told it may
// for the whole file; the form for NEON takes none of them.
        .arch_extension sve

// What the object formats want differently: the function's name, with the
// leading underscore of Mach-O there, and the names of labels local to the
// file, which Mach-O's assembler keeps out of the symbol table when they start
// with L and the others when they start with .L.
#if defined(__APPLE__)
#define LOCAL(name) L##name
#elif defined(__ELF__) || defined(_WIN32)
#define LOCAL(name) .L##name
#else
#error "an object format the pass is not written for"
#endif

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

        .text

// The 32-bit values of the eight lanes from lane \l at byte \offset, in 16 bits,
// into v\dst, with v\t: each held to 16 bits as the pass for AVX2 packs them
// (vpackusdw), which every one of them fits.
.macro PACK dst, offset, l, t
        ldr       q\dst, [x0, #(\offset + 4 * (\l))]
        ldr       q\t, [x0, #(\offset + 4 * (\l) + 16)]
        sqxtun    v\dst\().4h, v\dst\().4s
        sqxtun2   v\dst\().8h, v\t\().4s
.endm

// The envelopes of the eight lanes from lane \l (ymf262::envelope() and
// attenuation()): the state and the level for the next pass, and what the
// operator takes of this pass's level. The rate a state goes by is taken from
// its byte in 32 bits. Leaves dokon (keyed on from release) in 32 bits for the
// phases: lanes \l to \l + 3 in v3, \l + 4 to \l + 7 in v26. \second is the
// envelope timer's second half.
.macro ENVELOPE second, l
        ldr       q0, [x0, #(STATE16 + 2 * (\l))]                    // state
        ldr       q1, [x0, #(LEVEL16 + 2 * (\l))]                    // level
        PACK      2, KEY_ON, \l, 16
        neg       v2.8h, v2.8h                                      // keyed: all ones
        movi      v16.8h, #3
        cmeq      v3.8h, v0.8h, v16.8h
        and       v3.16b, v3.16b, v2.16b                            // dokon
        bic       v4.16b, v0.16b, v3.16b                            // the state the rate goes by
        ushll     v5.4s, v4.4h, #3
        ushll2    v4.4s, v4.8h, #3
        neg       v5.4s, v5.4s
        neg       v4.4s, v4.4s
        ldr       q6, [x0, #(RATES + 4 * (\l))]
        ldr       q7, [x0, #(RATES + 4 * (\l) + 16)]
        ushl      v5.4s, v6.4s, v5.4s
        ushl      v4.4s, v7.4s, v4.4s
        xtn       v18.4h, v5.4s
        xtn2      v18.8h, v4.4s
        bic       v18.8h, #0xff, lsl #8                            // its rate
        movi      v16.8h, #15
        and       v5.16b, v18.16b, v16.16b                          // rate_hi
        PACK      6, KEY_SCALE_LOW, \l, 17
        ldr       q17, [x1, #IN_HIGH_FROM_LESS_1]
        cmgt      v7.8h, v6.8h, v17.8h                              // stephi
.if \second
        movi      v17.8h, #12
        cmeq      v19.8h, v5.8h, v17.8h
        orr       v7.16b, v7.16b, v19.16b                           // high
.endif
        mvni      v17.8h, #11                                       // -12
        add       v19.8h, v5.8h, v17.8h
        sub       v19.8h, v19.8h, v7.8h
        movi      v17.8h, #3
        smin      v19.8h, v19.8h, v17.8h                            // the step from rate 12, none below
.if \second
        // The low step, below rate 12. Rates of 12 and up take a step of 1 at
        // least when the timer's second half is on, which the low step cannot
        // raise, so it is not asked whether the rate is below 12.
        ldr       q17, [x1, #IN_ADD]
        add       v7.8h, v5.8h, v17.8h
        and       v7.16b, v7.16b, v16.16b                           // sum
        ldr       q20, LOCAL(low_step)
        tbl       v7.16b, {v20.16b}, v7.16b
        ldr       q20, LOCAL(pow2)
        tbl       v6.16b, {v20.16b}, v6.16b                         // this key scaling's bit, and a byte above
        movi      v17.8h, #16
        and       v18.16b, v18.16b, v17.16b
        cmeq      v18.8h, v18.8h, v17.8h                            // the register's rate is not 0
        and       v6.16b, v6.16b, v18.16b
        and       v7.16b, v7.16b, v6.16b
        movi      v17.8h, #1
        umin      v7.8h, v7.8h, v17.8h                              // inclow
        smax      v19.8h, v19.8h, v7.8h                             // step
.else
        movi      v17.2d, #0
        smax      v19.8h, v19.8h, v17.8h                            // step
.endif
        cmeq      v20.8h, v5.8h, v16.8h                             // maxrate
        cmeq      v4.8h, v0.8h, #0                                  // attack
        cmeq      v5.8h, v1.8h, #0                                  // zeroreach
        PACK      6, SUSTAIN, \l, 17
        ushr      v7.8h, v1.8h, #4
        cmeq      v6.8h, v6.8h, v7.8h                               // slreach
        ldr       q17, LOCAL(w_0x1f7)
        cmgt      v21.8h, v1.8h, v17.8h                             // silent
        movi      v17.8h, #1
        cmeq      v22.8h, v0.8h, v17.8h                             // decay
        // The state for the next pass: one on from attack at level 0 and from
        // decay at the sustain level, release with the key off, attack on dokon.
        and       v23.16b, v22.16b, v6.16b
        and       v24.16b, v4.16b, v5.16b
        orr       v23.16b, v23.16b, v24.16b
        sub       v23.8h, v0.8h, v23.8h
        movi      v24.8h, #3
        bic       v24.16b, v24.16b, v2.16b
        orr       v23.16b, v23.16b, v24.16b
        bic       v23.16b, v23.16b, v3.16b
        str       q23, [x0, #(STATE16 + 2 * (\l))]
        cmgt      v23.8h, v0.8h, v17.8h                             // sustain or release
        bic       v24.16b, v22.16b, v6.16b                          // decay short of the sustain level
        orr       v23.16b, v23.16b, v24.16b
        orr       v24.16b, v3.16b, v21.16b
        bic       v23.16b, v23.16b, v24.16b                         // linear
        and       v24.16b, v4.16b, v2.16b
        orr       v25.16b, v20.16b, v5.16b
        bic       v24.16b, v24.16b, v25.16b                         // exponent
        and       v20.16b, v20.16b, v3.16b                          // instant
        orr       v4.16b, v4.16b, v3.16b
        bic       v4.16b, v21.16b, v4.16b                           // mute
        bic       v25.16b, v1.16b, v20.16b
        mvni      v16.8h, #0xfe, lsl #8                            // 0x1ff
        and       v4.16b, v4.16b, v16.16b
        orr       v25.16b, v25.16b, v4.16b                          // the level before the step
        ushr      v4.8h, v1.8h, #1
        movi      v17.8h, #0xff
        eor       v4.16b, v4.16b, v17.16b
        and       v4.16b, v4.16b, v24.16b
        movi      v17.8h, #4
        and       v23.16b, v23.16b, v17.16b
        orr       v4.16b, v4.16b, v23.16b                           // the increment
        mov       v23.16b, v19.16b
        orr       v23.8h, #0x04, lsl #8
        ldr       q17, LOCAL(attack_bits)
        tbl       v23.16b, {v17.16b}, v23.16b
        and       v23.16b, v23.16b, v24.16b                         // the attack's high bits
        // The increment shifted down by 3 less the step: up by the step, then
        // down by 3.
        ushl      v4.8h, v4.8h, v19.8h
        ushr      v4.8h, v4.8h, #3
        orr       v4.16b, v4.16b, v23.16b
        cmeq      v17.8h, v19.8h, #0
        bic       v4.16b, v4.16b, v17.16b                           // nothing without a step
        add       v4.8h, v4.8h, v25.8h
        and       v4.16b, v4.16b, v16.16b
        str       q4, [x0, #(LEVEL16 + 2 * (\l))]
        // The attenuation the operator takes: this pass's level with the key
        // scale level, the total level and the tremolo, all ones past nine bits.
        PACK      5, TREMOLO_DEPTH, \l, 17
        ldr       q17, [x1, #IN_TREMOLO]
        tbl       v5.16b, {v17.16b}, v5.16b                         // the tremolo
        PACK      6, LEVEL_OFFSET, \l, 17
        add       v5.8h, v5.8h, v6.8h
        and       v6.16b, v5.16b, v16.16b
        add       v6.8h, v6.8h, v1.8h
        movi      v17.8h, #0x02, lsl #8                            // 0x200
        and       v5.16b, v5.16b, v17.16b
        orr       v6.16b, v6.16b, v5.16b
        smin      v6.8h, v6.8h, v16.8h
        uxtl      v5.4s, v6.4h
        uxtl2     v6.4s, v6.8h
        str       q5, [x0, #(EG_OUT + 4 * (\l))]
        str       q6, [x0, #(EG_OUT + 4 * (\l) + 16)]
        sxtl2     v26.4s, v3.8h
        sxtl      v3.4s, v3.4h
.endm

// The phases of the four lanes from lane \l (ymf262::phase_increment(),
// phase_out() and next_phase()): what the operator reads of this pass's phase,
// and the phase for the next pass, from nought on dokon (v\restart).
.macro PHASE l, restart
        ldr       q4, [x0, #(VIBRATO_DEPTH + 4 * (\l))]
        ldr       q5, [x0, #(FNUM + 4 * (\l))]
        ushl      v6.4s, v5.4s, v4.4s
        ldr       q7, [x1, #IN_VIBRATO_SHIFT]
        neg       v7.4s, v7.4s
        ushl      v6.4s, v6.4s, v7.4s                               // fnum shifted down as the depth asks
        movi      v7.4s, #1
        eor       v4.16b, v4.16b, v7.16b
        neg       v4.4s, v4.4s
        ldr       q7, [x1, #IN_VIBRATO_MASK]
        ushl      v4.4s, v7.4s, v4.4s                               // the depth's bits
        and       v6.16b, v6.16b, v4.16b
        ldr       q4, [x0, #(VIBRATO + 4 * (\l))]
        neg       v4.4s, v4.4s
        and       v6.16b, v6.16b, v4.16b                            // the vibrato's swing
        ldr       q7, [x1, #IN_VIBRATO_SIGN]
        mul       v6.4s, v6.4s, v7.4s
        add       v5.4s, v5.4s, v6.4s
        ldr       q7, [x1, #IN_VIBRATO_WRAP]
        and       v5.16b, v5.16b, v7.16b                            // the frequency number
        ldr       q7, [x0, #(BLOCK + 4 * (\l))]
        ushl      v5.4s, v5.4s, v7.4s
        ushr      v5.4s, v5.4s, #1
        ldr       q7, [x0, #(MULTIPLE + 4 * (\l))]
        mul       v5.4s, v5.4s, v7.4s
        ushr      v5.4s, v5.4s, #1                                  // the increment
        ldr       q6, [x0, #(PHASE + 4 * (\l))]
        ushr      v7.4s, v6.4s, #9
        str       q7, [x0, #(PHASE_OUT + 4 * (\l))]
        bic       v6.16b, v6.16b, v\restart\().16b
        add       v6.4s, v6.4s, v5.4s
        movi      v7.4s, #0x07, msl #16                            // 0x7ffff
        and       v6.16b, v6.16b, v7.16b
        str       q6, [x0, #(PHASE + 4 * (\l))]
.endm

.macro ENVELOPES_AND_PHASES second, l
        ENVELOPE  \second, \l
        PHASE     \l, 3
        PHASE     \l + 4, 26
.endm

// The four lanes of v\idx looked up in the table at \table, into v\dst: in the
// form for SVE (\sve 1) by a gather under p0, four elements; in the other one by
// one, with w9 to w15.
.macro LOOKUP dst, idx, table, sve
.if \sve
        ld1w      {z\dst\().s}, p0/z, [\table, z\idx\().s, uxtw #2]
.else
        umov      w9, v\idx\().s[0]
        umov      w10, v\idx\().s[1]
        umov      w11, v\idx\().s[2]
        umov      w12, v\idx\().s[3]
        ldr       s\dst, [\table, w9, uxtw #2]
        ldr       w13, [\table, w10, uxtw #2]
        ldr       w14, [\table, w11, uxtw #2]
        ldr       w15, [\table, w12, uxtw #2]
        mov       v\dst\().s[1], w13
        mov       v\dst\().s[2], w14
        mov       v\dst\().s[3], w15
.endif
.endm

// The operators of the four lanes from lane \l (ymf262::modulation() and
// operator_output()), modulated, when they are by the stage before, by the
// lanes from lane \source.
.macro OPERATOR l, source, sve
        // First what does not wait for the stage before: the feedback, added to
        // the phase as the modulation, to which the stage before's modulation is
        // added later (no slot has both).
        ldr       q0, [x0, #(MODULATED_BY + 4 * (\l))]
        movi      v16.4s, #1
        cmeq      v1.4s, v0.4s, v16.4s
        movi      v17.4s, #0x03, msl #8                            // 1023
        and       v1.16b, v1.16b, v17.16b                           // what is taken of the stage before
        movi      v16.4s, #2
        cmeq      v0.4s, v0.4s, v16.4s
        ldr       q2, [x0, #(OUT_1 + 4 * (\l))]
        ldr       q16, [x0, #(OUT_2 + 4 * (\l))]
        add       v2.4s, v2.4s, v16.4s
        ldr       q16, [x0, #(FEEDBACK_SHIFT + 4 * (\l))]
        neg       v16.4s, v16.4s
        sshl      v2.4s, v2.4s, v16.4s
        and       v2.16b, v2.16b, v0.16b
        and       v2.16b, v2.16b, v17.16b                           // the feedback
        ldr       q16, [x0, #(PHASE_OUT + 4 * (\l))]
        add       v2.4s, v2.4s, v16.4s
        ldr       q5, [x0, #(TURN_BIT + 4 * (\l))]
        ldr       q3, [x0, #(SQUARE + 4 * (\l))]
        neg       v3.4s, v3.4s                                      // square: all ones
        // From the stage before to the log-sine's index.
        ldr       q16, [x0, #(OUT + 4 * (\source))]
        and       v1.16b, v1.16b, v16.16b
        add       v1.4s, v1.4s, v2.4s                               // the phase
        ldr       q16, [x0, #(PHASE_SHIFT + 4 * (\l))]
        ushl      v2.4s, v1.4s, v16.4s                              // shifted, with the bits above its ten
        and       v4.16b, v2.16b, v5.16b
        cmeq      v4.4s, v4.4s, v5.4s                               // turned back: all ones
        eor       v4.16b, v4.16b, v2.16b                            // phase2, in its low nine bits
        orr       v5.16b, v4.16b, v3.16b
        movi      v16.4s, #255
        and       v5.16b, v5.16b, v16.16b                           // the log-sine's index
        LOOKUP    7, 5, x3, \sve                                    // sine
        // While it loads: the ramp, the sign (bit 9 of shifted, which turning
        // back leaves alone) and the mute.
        ldr       q6, [x0, #(SAWTOOTH + 4 * (\l))]
        neg       v6.4s, v6.4s                                      // sawtooth: all ones
        movi      v16.4s, #0x01, msl #8                            // 511
        and       v4.16b, v4.16b, v16.16b
        shl       v4.4s, v4.4s, #3
        and       v4.16b, v4.16b, v6.16b                            // the ramp, for the sawtooth
        ldr       q16, [x0, #(SIGN_BIT + 4 * (\l))]
        and       v2.16b, v2.16b, v16.16b
        cmeq      v2.4s, v2.4s, #0                                  // positive
        ldr       q16, [x0, #(MUTE_BIT + 4 * (\l))]
        and       v1.16b, v1.16b, v16.16b
        cmeq      v1.4s, v1.4s, #0                                  // heard
        bic       v2.16b, v1.16b, v2.16b                            // heard and negative
        ldr       q0, [x0, #(EG_OUT + 4 * (\l))]
        shl       v0.4s, v0.4s, #3
        // From the sine to the exponent's index. att is not held to 4095: past
        // it, it shifts the magnitude out all the same.
        bic       v7.16b, v7.16b, v6.16b
        orr       v7.16b, v7.16b, v4.16b                            // shape
        add       v7.4s, v7.4s, v0.4s                               // att
        movi      v16.4s, #255
        and       v3.16b, v7.16b, v16.16b
        LOOKUP    5, 3, x4, \sve
        ushr      v7.4s, v7.4s, #8
        neg       v7.4s, v7.4s
        ushl      v5.4s, v5.4s, v7.4s                               // magnitude, below 4096
        // The value: nought when muted, and when negative the magnitude's
        // complement, which is the 13-bit ones' complement taken as signed.
        and       v5.16b, v5.16b, v1.16b
        eor       v5.16b, v5.16b, v2.16b
        str       q5, [x0, #(OUT + 4 * (\l))]
.endm

// The ring of the hi-hat's and the top cymbal's bits, into w9, with w10 and
// w11.
.macro RING
        ldr       w9, [x0, #HH2]
        ldr       w10, [x0, #HH7]
        eor       w9, w9, w10
        ldr       w10, [x0, #TC5]
        ldr       w11, [x0, #HH3]
        eor       w10, w10, w11
        orr       w9, w9, w10
        ldr       w10, [x0, #TC5]
        ldr       w11, [x0, #TC3]
        eor       w10, w10, w11
        orr       w9, w9, w10
.endm

// The hi-hat (slot 13, lane 7, Core::rhythm_phases()): its bits are taken every
// pass, and in rhythm mode it takes a phase of its own, by the top cymbal's bits
// of the last time they were taken.
.macro HI_HAT
        ldr       w12, [x0, #(PHASE_OUT + 28)]
        ubfx      w13, w12, #2, #1
        str       w13, [x0, #HH2]
        ubfx      w13, w12, #3, #1
        str       w13, [x0, #HH3]
        ubfx      w13, w12, #7, #1
        str       w13, [x0, #HH7]
        ubfx      w13, w12, #8, #1
        str       w13, [x0, #HH8]
        ldr       w13, [x1, #IN_RHYTHM_13]
        cbz       w13, 1f
        RING
        ldr       w10, [x1, #IN_HI_HAT_NOISE]
        eor       w10, w9, w10
        mov       w11, #(0xd0 - 0x34)
        mul       w10, w10, w11
        add       w10, w10, #0x34
        orr       w9, w10, w9, lsl #9
        str       w9, [x0, #(PHASE_OUT + 28)]
1:
.endm

// The snare (slot 16, lane 23) and the top cymbal (slot 17, lane 24), by the
// hi-hat's bits of this pass.
.macro SNARE_AND_TOP_CYMBAL
        ldr       w13, [x1, #IN_RHYTHM_16]
        cbz       w13, 1f
        ldr       w9, [x0, #HH8]
        ldr       w10, [x1, #IN_SNARE_NOISE]
        eor       w10, w9, w10
        lsl       w10, w10, #8
        orr       w9, w10, w9, lsl #9
        str       w9, [x0, #(PHASE_OUT + 92)]
1:
        ldr       w13, [x1, #IN_RHYTHM_17]
        cbz       w13, 2f
        ldr       w12, [x0, #(PHASE_OUT + 96)]
        ubfx      w13, w12, #3, #1
        str       w13, [x0, #TC3]
        ubfx      w13, w12, #5, #1
        str       w13, [x0, #TC5]
        RING
        lsl       w9, w9, #9
        orr       w9, w9, #0x80
        str       w9, [x0, #(PHASE_OUT + 96)]
2:
.endm

// The feedback history of the slots that share a queue (\q: 0 for slots 13 and
// 31, lanes 7 and 10; 1 for 14 and 32, lanes 8 and 11; Core::operators()): slot
// m reads the second and the fourth value, slot m + 18 the first and the third,
// or the same two as m when m's value is held out of the queue.
.macro SHARED_READ q, low, high, hold
        ldr       w9, [x0, #(SHARED_HISTORY + 16 * \q + 4)]
        str       w9, [x0, #(OUT_1 + 4 * \low)]
        ldr       w10, [x0, #(SHARED_HISTORY + 16 * \q)]
        ldr       w11, [x1, #\hold]
        cmp       w11, #0
        csel      w10, w9, w10, ne
        str       w10, [x0, #(OUT_1 + 4 * \high)]
        ldr       w9, [x0, #(SHARED_HISTORY + 16 * \q + 12)]
        str       w9, [x0, #(OUT_2 + 4 * \low)]
        ldr       w10, [x0, #(SHARED_HISTORY + 16 * \q + 8)]
        csel      w10, w9, w10, ne
        str       w10, [x0, #(OUT_2 + 4 * \high)]
.endm

// The values of this pass into the queue (Core::store_history()): slot m's but
// when it is held out, then slot m + 18's.
.macro SHARED_STORE q, low, high, hold
        ldr       w9, [x0, #(SHARED_HISTORY + 16 * \q)]
        ldr       w10, [x0, #(SHARED_HISTORY + 16 * \q + 4)]
        ldr       w11, [x0, #(SHARED_HISTORY + 16 * \q + 8)]
        ldr       w12, [x1, #\hold]
        cbnz      w12, 1f
        mov       w11, w10
        mov       w10, w9
        ldr       w9, [x0, #(OUT + 4 * \low)]
1:
        ldr       w12, [x0, #(OUT + 4 * \high)]
        str       w12, [x0, #(SHARED_HISTORY + 16 * \q)]
        str       w9, [x0, #(SHARED_HISTORY + 16 * \q + 4)]
        str       w10, [x0, #(SHARED_HISTORY + 16 * \q + 8)]
        str       w11, [x0, #(SHARED_HISTORY + 16 * \q + 12)]
.endm

// The last two values of the eight lanes from lane \l, by x5, which points at
// OUT, with OUT_1 and OUT_2 within the reach of a pair's offset.
.macro HISTORY l
        ldp       q16, q17, [x5, #(OUT_1 - OUT + 4 * (\l))]
        stp       q16, q17, [x5, #(OUT_2 - OUT + 4 * (\l))]
        ldp       q16, q17, [x5, #(4 * (\l))]
        stp       q16, q17, [x5, #(OUT_1 - OUT + 4 * (\l))]
.endm

// The products of the values of the four lanes from lane \l and their weights
// on the outputs, added into v16 to v19: a value fits 16 bits and a weight is 2
// at the most, so each lane's sum fits 32 bits.
.macro HEARD l
        ldr       q0, [x0, #(OUT + 4 * (\l))]
        ldr       q1, [x0, #(A_NOW + 4 * (\l))]
        mla       v16.4s, v0.4s, v1.4s
        ldr       q1, [x0, #(A_LATE + 4 * (\l))]
        mla       v17.4s, v0.4s, v1.4s
        ldr       q1, [x0, #(B_NOW + 4 * (\l))]
        mla       v18.4s, v0.4s, v1.4s
        ldr       q1, [x0, #(B_LATE + 4 * (\l))]
        mla       v19.4s, v0.4s, v1.4s
.endm

// The same with soft panning, whose weights reach 2^17: each product in 64
// bits, added into v16 to v23, two for each output's sum, the lanes' low halves
// and their high halves.
.macro HEARD_SOFT l
        ldr       q0, [x0, #(OUT + 4 * (\l))]
        ldr       q1, [x0, #(SOFT_A_NOW + 4 * (\l))]
        smlal     v16.2d, v0.2s, v1.2s
        smlal2    v17.2d, v0.4s, v1.4s
        ldr       q1, [x0, #(SOFT_A_LATE + 4 * (\l))]
        smlal     v18.2d, v0.2s, v1.2s
        smlal2    v19.2d, v0.4s, v1.4s
        ldr       q1, [x0, #(SOFT_B_NOW + 4 * (\l))]
        smlal     v20.2d, v0.2s, v1.2s
        smlal2    v21.2d, v0.4s, v1.4s
        ldr       q1, [x0, #(SOFT_B_LATE + 4 * (\l))]
        smlal     v22.2d, v0.2s, v1.2s
        smlal2    v23.2d, v0.4s, v1.4s
.endm

// \r, of 64 bits, shifted down by 16 and rounded half away from nought.
.macro ROUND16 r
        cmp       \r, #0
        cneg      \r, \r, lt
        add       \r, \r, #8, lsl #12
        lsr       \r, \r, #16
        cneg      \r, \r, lt
.endm

// \r, of 32 bits, in the nineteen bits the chip adds in, then held to sixteen,
// with \t.
.macro CLIP r, t
        sbfx      \r, \r, #0, #19
        mov       \t, #32767
        cmp       \r, \t
        csel      \r, \t, \r, gt
        mov       \t, #-32768
        cmp       \r, \t
        csel      \r, \t, \r, lt
.endm

// The envelopes and phases and the operators, for a pass with the envelope
// timer's second half off (\second 0) or on (1). Each stage's envelopes and
// phases come before it, and the next stage's after it: stage 0 is lanes 0 to
// 15, stage 1 lanes 16 to 31, modulated by stage 0, stage 2 lanes 32 to 39 and
// stage 3 lanes 40 to 47, modulated by stages 1 and 2. The hi-hat's phase comes
// before stage 0, and the snare's and the top cymbal's before stage 1. \sve is
// the form (LOOKUP).
.macro PASS second, sve
        ENVELOPES_AND_PHASES \second, 0
        ENVELOPES_AND_PHASES \second, 8
        HI_HAT
        OPERATOR  0, 0, \sve
        OPERATOR  4, 4, \sve
        OPERATOR  8, 8, \sve
        OPERATOR  12, 12, \sve
        ENVELOPES_AND_PHASES \second, 16
        ENVELOPES_AND_PHASES \second, 24
        SNARE_AND_TOP_CYMBAL
        OPERATOR  16, 0, \sve
        OPERATOR  20, 4, \sve
        OPERATOR  24, 8, \sve
        OPERATOR  28, 12, \sve
        ENVELOPES_AND_PHASES \second, 32
        ENVELOPES_AND_PHASES \second, 40
        OPERATOR  32, 16, \sve
        OPERATOR  36, 20, \sve
        OPERATOR  40, 32, \sve
        OPERATOR  44, 36, \sve
.endm

// The pass \name, in the form for SVE (\sve 1) or for NEON. Its labels are 7 to
// 9, which none of the macros it takes uses.
.macro PASS_FUNCTION name, sve
        .p2align  5
#if defined(__APPLE__)
        .globl    _\name
        .private_extern _\name
_\name:
#elif defined(_WIN32)
        .globl    \name
        .def      \name
        .scl      2
        .type     32
        .endef
\name:
#else
        .globl    \name
        .hidden   \name
        .type     \name, %function
\name:
#endif
.if \sve
        ptrue     p0.s, vl4
.endif
        SHARED_READ 0, 7, 10, IN_HOLD_13
        SHARED_READ 1, 8, 11, IN_HOLD_14
        ldr       x3, [x1, #IN_LOGSIN]
        ldr       x4, [x1, #IN_EXP]
        ldr       w9, [x1, #IN_EG_SECOND]
        cbnz      w9, 7f
        PASS      0, \sve
        b         8f
7:
        PASS      1, \sve
8:
        add       x5, x0, #(OUT & 0xfff)
        add       x5, x5, #(OUT >> 12), lsl #12
        HISTORY   0
        HISTORY   8
        HISTORY   16
        HISTORY   24
        HISTORY   32
        HISTORY   40
        SHARED_STORE 0, 7, 10, IN_HOLD_13
        SHARED_STORE 1, 8, 11, IN_HOLD_14
        movi      v16.2d, #0
        movi      v17.2d, #0
        movi      v18.2d, #0
        movi      v19.2d, #0
        ldr       w9, [x1, #IN_SOFT_PAN]
        cbnz      w9, 9f
        // Core::accumulate() without soft panning.
        HEARD     0
        HEARD     4
        HEARD     8
        HEARD     12
        HEARD     16
        HEARD     20
        HEARD     24
        HEARD     28
        HEARD     32
        HEARD     36
        HEARD     40
        HEARD     44
        addv      s16, v16.4s
        addv      s17, v17.4s
        addv      s18, v18.4s
        addv      s19, v19.4s
        fmov      w9, s16                                           // a now
        fmov      w10, s17                                          // a late
        fmov      w11, s18                                          // b now
        fmov      w12, s19                                          // b late
        ldr       w13, [x0, #CARRY]
        add       w9, w9, w13
        ldr       w13, [x0, #(CARRY + 4)]
        add       w11, w11, w13
        CLIP      w9, w13
        CLIP      w11, w13
        str       w9, [x2]
        str       w11, [x2, #4]
        str       w10, [x0, #CARRY]
        str       w12, [x0, #(CARRY + 4)]
        sbfiz     x10, x10, #16, #32
        sbfiz     x12, x12, #16, #32
        str       x10, [x0, #CARRY_SOFT]
        str       x12, [x0, #(CARRY_SOFT + 8)]
        ret
9:
        // With soft panning: the sums exact in 64 bits, and one rounding at
        // the end.
        movi      v20.2d, #0
        movi      v21.2d, #0
        movi      v22.2d, #0
        movi      v23.2d, #0
        HEARD_SOFT 0
        HEARD_SOFT 4
        HEARD_SOFT 8
        HEARD_SOFT 12
        HEARD_SOFT 16
        HEARD_SOFT 20
        HEARD_SOFT 24
        HEARD_SOFT 28
        HEARD_SOFT 32
        HEARD_SOFT 36
        HEARD_SOFT 40
        HEARD_SOFT 44
        add       v16.2d, v16.2d, v17.2d
        add       v18.2d, v18.2d, v19.2d
        add       v20.2d, v20.2d, v21.2d
        add       v22.2d, v22.2d, v23.2d
        addp      d16, v16.2d
        addp      d18, v18.2d
        addp      d20, v20.2d
        addp      d22, v22.2d
        fmov      x9, d16                                           // a now
        fmov      x10, d18                                          // a late
        fmov      x11, d20                                          // b now
        fmov      x12, d22                                          // b late
        ldr       x13, [x0, #CARRY_SOFT]
        add       x9, x9, x13
        str       x10, [x0, #CARRY_SOFT]
        ROUND16   x9
        CLIP      w9, w13
        str       w9, [x2]
        ROUND16   x10
        str       w10, [x0, #CARRY]
        ldr       x13, [x0, #(CARRY_SOFT + 8)]
        add       x11, x11, x13
        str       x12, [x0, #(CARRY_SOFT + 8)]
        ROUND16   x11
        CLIP      w11, w13
        str       w11, [x2, #4]
        ROUND16   x12
        str       w12, [x0, #(CARRY + 4)]
        ret
#if defined(__ELF__)
        .size     \name, . - \name
#endif
.endm

        PASS_FUNCTION adlplug_opl3_pass_neon, 0
        PASS_FUNCTION adlplug_opl3_pass_sve, 1

// The tables of tbl, sixteen bytes each, beside the code, which loads them by
// their addresses from its own (ldr of a literal): no relocation, in any of
// the object formats.
        .p2align  4
// By the sum of a rate and the timer's step (0 to 15): the key scalings, a bit
// for each value of their low two bits, for which the low step falls.
LOCAL(low_step):
        .byte     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x0f, 0x0c, 0x0a, 0
// By a step (0 to 3): the low byte of the attack's high bits, and at 4 their
// 0x100, which the index's high byte takes.
LOCAL(attack_bits):
        .byte     0x40, 0xc0, 0x80, 0x00, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
// By 0 to 3: 1 shifted up by it.
LOCAL(pow2):
        .byte     1, 2, 4, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
// Eight 16-bit lanes of 0x1f7, the level past which the envelope is silent.
LOCAL(w_0x1f7):
        .rept     8
        .short    0x1f7
        .endr

#if defined(__ELF__)
        .section  .note.GNU-stack, "", %progbits
#endif
