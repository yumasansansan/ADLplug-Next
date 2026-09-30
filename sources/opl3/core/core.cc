// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// ADLplug-Next's emulator core of the YMF262 (OPL3); see core.h.
//
// Time is counted in passes over the 36 slots, one a sample, and in slot times
// within a pass. The chip works a slot out over several slot times; slot time
// s is the one at which the chip works out slot s's phase and envelope.

#include "core.h"

#include <cmath>

// Every loop over the lanes is vectorized. The pragma asks for it whatever the
// cost model makes of it (for x86-64-v3 it would leave the loops that look up
// tables scalar, and they take longer so), and a loop the compiler cannot
// vectorize is a warning, which the project's -Werror makes an error. The loop
// is also kept from being unrolled before the vectorizer sees it: a short one,
// like a stage's 8 lanes, would become straight-line code, with no loop left to
// vectorize or to warn about. The undefined behaviour sanitizer puts its checks
// in the loops, and they branch, which no vectorized loop can: under it the
// loops are only kept from unrolling, and run scalar with every check.
//
// A loop over the lanes reads what a lane goes by before it chooses anything,
// and chooses between names or constants (ymf262_logic.h), so that nothing is
// read behind a condition: such a read (the right of an && or one side of a
// ?:) is a masked load in a vectorized loop, and for bytes, which no x86
// instruction loads masked, a branch for each lane.
#if __has_feature(undefined_behavior_sanitizer)
#define OPL3_VECTOR_LOOP _Pragma("clang loop unroll(disable)")
#else
#define OPL3_VECTOR_LOOP _Pragma("clang loop vectorize(enable) unroll(disable)")
#endif

namespace adlplug::opl3 {
namespace {

constexpr std::uint8_t pad = 0xff;

// Lane by lane, the slot, or pad for a lane that has none.
constexpr std::array<std::uint8_t, 48> slot_of = {
    0, 1, 2, 18, 19, 20, 12, 13, 14, 30, 31, 32, pad, pad, pad, pad,  // the first operator of every chain
    3, 4, 5, 21, 22, 23, 15, 16, 17, 33, 34, 35, pad, pad, pad, pad,  // the second
    6, 7, 8, 24, 25, 26, pad, pad,                                    // the third of a four-operator chain
    9, 10, 11, 27, 28, 29, pad, pad,                                  // and its fourth
};

// The lanes of the rhythm slots 13, 14, 16 and 17, and of 31 and 32, which
// share their feedback store with 13 and 14.
constexpr std::size_t lane_13 = 7, lane_14 = 8, lane_16 = 23, lane_17 = 24;
constexpr std::size_t lane_31 = 10, lane_32 = 11;

// The slot a register address below 0x20 names, from the low five bits.
constexpr std::array<int, 32> slot_of_offset = {
    0, 1, 2, 3, 4, 5, -1, -1, 6, 7, 8, 9, 10, 11, -1, -1,
    12, 13, 14, 15, 16, 17, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

// Where a slot is: its bank, and its row of the bank's 18. Row r is in
// channel (r / 6) * 3 + r % 3 of the bank, as its first operator for r % 6
// below 3 and as its second from 3. The rows below 12 make the bank's three
// chains of four, chain r % 3 of channels r % 3 and r % 3 + 3, as A, B, C and
// D by r / 3.
constexpr int bank_of(int slot) { return slot / 18; }
constexpr int row_of(int slot) { return slot % 18; }
constexpr int channel_of(int slot) { return bank_of(slot) * 9 + row_of(slot) / 6 * 3 + row_of(slot) % 3; }
constexpr bool is_first_operator(int slot) { return row_of(slot) % 6 < 3; }
constexpr int chain_of(int slot) { return row_of(slot) < 12 ? bank_of(slot) * 3 + row_of(slot) % 3 : -1; }

// The lane of each slot, the lanes of each channel's two slots (the first
// operator's first), and those of each chain's four (A, B, C, D).
constexpr std::array<std::uint8_t, 36> lane_of_slot = [] {
    std::array<std::uint8_t, 36> t{};
    for (std::size_t lane = 0; lane < slot_of.size(); ++lane)
        if (slot_of[lane] != pad)
            t[slot_of[lane]] = static_cast<std::uint8_t>(lane);
    return t;
}();

constexpr std::array<std::array<std::uint8_t, 2>, 18> lanes_of_channel = [] {
    std::array<std::array<std::uint8_t, 2>, 18> t{};
    for (int slot = 0; slot < 36; ++slot)
        t[static_cast<std::size_t>(channel_of(slot))][is_first_operator(slot) ? 0 : 1] =
            lane_of_slot[static_cast<std::size_t>(slot)];
    return t;
}();

constexpr std::array<std::array<std::uint8_t, 4>, 6> lanes_of_chain = [] {
    std::array<std::array<std::uint8_t, 4>, 6> t{};
    for (int slot = 0; slot < 36; ++slot)
        if (chain_of(slot) >= 0)
            t[static_cast<std::size_t>(chain_of(slot))][static_cast<std::size_t>(row_of(slot) / 3)] =
                lane_of_slot[static_cast<std::size_t>(slot)];
    return t;
}();

// The chip sums a sample of output A from slot 33 of one pass to slot 32 of the
// next, and a sample of output B from slot 15 to slot 14: the slots from these
// on are heard in the sample after the one of their pass.
constexpr int first_late_on_a = 33, first_late_on_b = 15;

// libADLMIDI's wrapper of the low-level core writes the address of a register,
// 8 slot times later its data, and 72 slot times after that the next address.
// Its sample begins 3 1/4 slot times before the end of the pass generate()
// works out, and a data write reaches the chip's registers 1 3/4 slot times
// after it is made: a data write at slot time k of the sample takes effect at
// slot time k + 34 of that pass, or at k - 2 of the next.
constexpr int address_to_data = 8, data_to_address = 72;

// When a slot reads what it goes by, in slot times from its own slot time: its
// own registers and its channel's the slot time before, its channel's feedback
// four before, and the registers of the whole chip at its own.
constexpr int reads_own = -1, reads_feedback = -4, reads_chip = 0;

// The rhythm flag is read at slot time 12 for all six drums' keys, the slot
// time before a slot for stopping its feedback or modulation, at its own for
// its phase, and two after for whether and how loud it is heard (and whether
// its value is stored for feedback).
constexpr int rhythm_keys_time = 12, rhythm_before = -1, rhythm_after = 2;

// Each chain of four operators has its flag read once a pass, at slot time 5,
// 6 or 7 for the first bank's chains and 23, 24 or 25 for the second's.
constexpr int chain_time(int slot)
{
    if (chain_of(slot) < 0)
        return slot;  // no chain
    return bank_of(slot) * 18 + 5 + row_of(slot) % 3;
}

// A table by lane of what f makes of the lane's slot, 0 for a pad.
template <typename F>
constexpr std::array<std::int32_t, 48> by_lane(F f)
{
    std::array<std::int32_t, 48> t{};
    for (std::size_t lane = 0; lane < t.size(); ++lane)
        t[lane] = slot_of[lane] == pad ? 0 : f(int{slot_of[lane]});
    return t;
}

// By lane: the slot time; that of the channel's first slot, and that of the
// chain's first slot (the slot's own out of a chain); the slot time the
// chain's flag is read at; the chain's bit in the register of the chains (0
// out of one); whether the slot is its channel's first, and whether it is in
// the second channel of a chain; and the bit of 0xbd that also keys it in
// rhythm mode (slots 12 to 17: the bass drum's two, the hi-hat, the tom, the
// snare and the top cymbal).
constexpr auto slot_time = by_lane([](int s) { return s; });
constexpr auto channel_first_time = by_lane([](int s) { return is_first_operator(s) ? s : s - 3; });
constexpr auto chain_first_time = by_lane([](int s) { return chain_of(s) < 0 ? s : bank_of(s) * 18 + row_of(s) % 3; });
constexpr auto chain_read_time = by_lane(chain_time);
constexpr auto chain_bit = by_lane([](int s) { return chain_of(s) < 0 ? 0 : 1 << chain_of(s); });
constexpr auto first_operator = by_lane([](int s) { return is_first_operator(s) ? 1 : 0; });
constexpr auto chain_second_channel = by_lane([](int s) { return chain_of(s) >= 0 && row_of(s) >= 6 ? 1 : 0; });
constexpr auto drum_phase = by_lane([](int s) { return s == 13 ? 1 : s == 16 ? 2 : s == 17 ? 3 : 0; });
constexpr auto drum_key_bit = by_lane([](int s) {
    constexpr std::array<int, 6> key_bit = {0x10, 0x01, 0x04, 0x10, 0x08, 0x02};
    return s >= 12 && s < 18 ? key_bit[static_cast<std::size_t>(s - 12)] : 0;
});

// The wrapper's reset leaves the chip with its counters where 100 passes after
// the reset puts them, and the noise 1784 steps past zero as the first pass
// after it begins.
constexpr int passes_after_reset = 100;
constexpr int noise_steps_after_reset = 1784;

constexpr ymf262::Counters counters_after_reset = [] {
    ymf262::Counters c;
    for (int i = 0; i < passes_after_reset; ++i)
        c.advance();
    return c;
}();

constexpr std::uint32_t noise_after_reset = [] {
    std::uint32_t s = 0;
    for (int i = 0; i < noise_steps_after_reset; ++i)
        s = ymf262::noise_step(s);
    return s;
}();

}  // namespace

void Core::reset()
{
    queue_head_ = queue_count_ = queue_wait_ = 0;
    landing_next_ = Landing{};
    r20_.fill(0);
    r40_.fill(0);
    r60_.fill(0);
    r80_.fill(0);
    re0_.fill(0);
    fnum_.fill(0);
    block_.fill(0);
    key_.fill(0);
    con_.fill(0);
    con_pair_.fill(0);
    fb_.fill(0);
    pan_.fill(3);
    newm_ = four_op_ = nts_ = rhythm_ = drum_keys_ = dam_ = dvb_ = 0;
    array_bank_ = array_reg_ = 0;
    array_value_ = 0;
    for (std::size_t channel = 0; channel < channels; ++channel)
        spread(channel);
    // The lanes' constants that the routing and derive() compare (core.h).
    for (std::size_t i = 0; i < lanes; ++i) {
        lane_slot_[i] = slot_of[i] == pad ? -1 : int{slot_of[i]};
        lane_first_[i] = first_operator[i];
        lane_second_channel_[i] = chain_second_channel[i];
    }
    phase_.fill(0);
    level_.fill(0x1ff);
    state_.fill(ymf262::release);
    out_.fill(0);
    out_1_.fill(0);
    out_2_.fill(0);
    shared_history_ = {};
    eg_out_.fill(0x1ff);
    phase_out_.fill(0);
    gain_a_.fill(65536);
    gain_b_.fill(65536);
    soft_pan_ = false;
    counters_ = counters_after_reset;
    noise_ = noise_after_reset;
    rhythm_bits_ = ymf262::Rhythm_bits{};
    a_carry_ = b_carry_ = 0;
    a_carry_soft_ = b_carry_soft_ = 0;
    landed_age_ = 0;
    landed_slot_time_ = -4;  // everything read afresh
    relatch();
    landed_age_ = -1;
}

void Core::write(std::uint16_t address, std::uint8_t value)
{
    if (queue_count_ == queue_size) {
        // Full: the oldest write takes effect at once.
        const Pending &p = queue_[static_cast<std::size_t>(queue_head_)];
        apply(p.address, p.value);
        landed_age_ = 0;
        landed_slot_time_ = -4;
        queue_head_ = (queue_head_ + 1) % queue_size;
        --queue_count_;
    }
    const int end = (queue_head_ + queue_count_) % queue_size;
    queue_[static_cast<std::size_t>(end)] = {.address = address, .value = value, .address_written = false};
    ++queue_count_;
}

void Core::write_pan(std::uint16_t address, std::uint8_t value)
{
    const int c = address & 0x0f;
    if (c > 8)
        return;
    const int channel_number = ((address >> 8) & 1) * 9 + c;
    const auto channel = static_cast<std::size_t>(channel_number);
    const double pi = std::acos(-1.0);
    const int v = value & 0x7f;
    // The side the channel is panned towards stays at full; the other falls
    // away along a quarter of a sine.
    double a = 1.0, b = 1.0;
    if (v < 64)
        b = std::sin(pi / 2 * v / 64.0);
    else if (v > 64)
        a = std::sin(pi / 2 * (127 - v) / 63.0);
    for (const std::size_t lane : lanes_of_channel[channel]) {
        gain_a_[lane] = static_cast<std::int32_t>(std::lround(a * 65536.0));
        gain_b_[lane] = static_cast<std::int32_t>(std::lround(b * 65536.0));
    }
    int panned = 0;
    OPL3_VECTOR_LOOP
    for (std::size_t i = 0; i < lanes; ++i) {
        const bool full_a = gain_a_[i] == 65536, full_b = gain_b_[i] == 65536;
        panned += full_a && full_b ? 0 : 1;
    }
    soft_pan_ = panned != 0;
    derive_soft_pan();
}

// The queue, over the sample this call stands for: the write, if any, whose
// data reaches the chip in the pass worked out now, and the one whose data
// reaches it in the next.
void Core::schedule_writes(Landing &now, Landing &next)
{
    int t = queue_wait_;
    while (queue_count_ > 0 && t < slots) {
        Pending &p = queue_[static_cast<std::size_t>(queue_head_)];
        if (!p.address_written) {
            p.address_written = true;
            t += address_to_data;
            continue;
        }
        Landing &l = t < 2 ? now : next;
        l = {.due = true, .address = p.address, .value = p.value, .slot_time = t < 2 ? t + 34 : t - 2};
        queue_head_ = (queue_head_ + 1) % queue_size;
        --queue_count_;
        t += data_to_address;
    }
    queue_wait_ = t > slots ? t - slots : 0;
}

void Core::land(const Landing &l)
{
    apply(l.address, l.value);
    landed_age_ = 0;
    landed_slot_time_ = l.slot_time;
}

// A write as it reaches the chip's registers.
void Core::apply(std::uint16_t address, std::uint8_t value)
{
    const int reg = address & 0xff;
    // The second bank is there only in the OPL3's own mode, but for the
    // register that turns that mode on; before it, its addresses are the
    // first bank's.
    const int bank = newm_ != 0 ? (address >> 8) & 1 : 0;
    if (reg < 0x20) {
        if (reg == 0x05 && (address & 0x100) != 0)
            newm_ = value & 1;
        else if (bank == 1 && reg == 0x04) {
            four_op_ = value & 0x3f;
            for (std::size_t channel = 0; channel < channels; ++channel)
                spread(channel);
        }
        else if (bank == 0 && reg == 0x08)
            nts_ = (value >> 6) & 1;
        // The chip then writes the last data written to the registers from
        // 0x20 on once more, to the same register, decoded afresh.
        apply_array(array_bank_, array_reg_, array_value_);
        return;
    }
    array_bank_ = bank;
    array_reg_ = reg;
    array_value_ = value;
    if (reg == 0xbd && bank == 0) {
        dam_ = (value >> 7) & 1;
        dvb_ = (value >> 6) & 1;
        rhythm_ = (value >> 5) & 1;
        drum_keys_ = value & 0x1f;
    }
    apply_array(bank, reg, value);
}

// A write to the registers of the operators and the channels.
void Core::apply_array(int bank, int reg, std::uint8_t value)
{
    if (reg >= 0xa0 && reg < 0xd0) {
        // The chip decodes a channel number of 9 as 7 and 10 as 8, and
        // ignores those above; but for the feedback, and the connection as a
        // chain of four reads it, of 0xc0 it takes 12, 13 and 14 as the first
        // bank's channels 3, 4 and 5, whichever bank they are written to.
        int c = reg & 0x0f;
        if ((reg & 0xf0) == 0xc0 && c >= 12 && c <= 14) {
            const auto first_bank = static_cast<std::size_t>(c - 9);
            con_pair_[first_bank] = value & 1;
            fb_[first_bank] = (value >> 1) & 7;
            spread(first_bank);
            return;
        }
        if (c == 9 || c == 10)
            c -= 2;
        else if (c > 8)
            return;
        const int channel = bank * 9 + c;
        const auto ch = static_cast<std::size_t>(channel);
        switch (reg & 0xf0) {
        case 0xa0:
            fnum_[ch] = static_cast<std::uint16_t>((fnum_[ch] & 0x300) | value);
            break;
        case 0xb0:
            fnum_[ch] = static_cast<std::uint16_t>((fnum_[ch] & 0xff) | ((value & 3) << 8));
            block_[ch] = (value >> 2) & 7;
            key_[ch] = (value >> 5) & 1;
            break;
        default:  // 0xc0
            con_[ch] = value & 1;
            con_pair_[ch] = value & 1;
            fb_[ch] = (value >> 1) & 7;
            pan_[ch] = newm_ != 0 ? ((value >> 4) & 15) : 3;
            break;
        }
        spread(ch);
        if (c < 3)
            spread(ch + 3);
        return;
    }
    const int s = slot_of_offset[static_cast<std::size_t>(reg & 0x1f)];
    if (s < 0 || (reg & 0x1f) > 0x15)
        return;
    const int slot = bank * 18 + s;
    const std::size_t lane = lane_of_slot[static_cast<std::size_t>(slot)];
    switch (reg & 0xe0) {
    case 0x20: r20_[lane] = value; break;
    case 0x40: r40_[lane] = value; break;
    case 0x60: r60_[lane] = value; break;
    case 0x80: r80_[lane] = value; break;
    case 0xe0: re0_[lane] = static_cast<std::uint8_t>(value & (newm_ != 0 ? 7 : 3)); break;
    default: break;
    }
}

// A channel's registers, into the lanes of the slots that read them: its own
// two slots', which in the second channel of a chain of four go by the first
// channel's frequency, block and key; and in a chain, the chain's four, which
// route by the first channel's connection and by the second's as a chain reads
// it. A write to the first channel of a chain spreads the second as well, and
// a write to the register of the chains spreads every channel.
void Core::spread(std::size_t channel)
{
    const std::size_t c = channel % 9, chain = channel / 9 * 3 + c % 3;
    const bool paired = c >= 3 && c < 6 && ((four_op_ >> chain) & 1) != 0;
    const std::size_t by = paired ? channel - 3 : channel;  // whose frequency, block and key
    for (const std::size_t lane : lanes_of_channel[channel]) {
        ch_fnum_[lane] = fnum_[by];
        ch_block_[lane] = block_[by];
        ch_key_[lane] = key_[by];
        ch_con_[lane] = con_[channel];
        ch_fb_[lane] = fb_[channel];
        ch_pan_[lane] = pan_[channel];
    }
    if (c < 3) {
        for (const std::size_t lane : lanes_of_chain[chain])
            first_con_[lane] = con_[channel];
    }
    else if (c < 6) {
        for (const std::size_t lane : lanes_of_chain[chain])
            second_con_[lane] = con_pair_[channel];
    }
}

namespace {

// What a lane holds after a reading: the register as it stands if the slot
// reads it after the write, or else what it held. Both are loaded, and the
// value chosen, rather than the one place to load from.
template <typename T>
[[gnu::always_inline]] constexpr T latch(bool reads, T now, T held)
{
    return reads ? now : held;
}

}  // namespace

// What each slot reads for the pass worked out now: the registers as they
// stand if it reads them after the last write took effect, or else what it
// read before.
void Core::relatch()
{
    const int since = 36 * landed_age_ - landed_slot_time_;  // slot times from the write to this pass
    OPL3_VECTOR_LOOP
    for (std::size_t i = 0; i < lanes; ++i) {
        const int s = slot_time[i];
        const bool own = since + s + reads_own >= 0, chip = since + s + reads_chip >= 0;
        const bool channel_first = since + channel_first_time[i] + reads_own >= 0;
        const bool chain_first = since + chain_first_time[i] + reads_own >= 0;
        l20_[i] = latch(own, r20_[i], l20_[i]);
        l40_[i] = latch(own, r40_[i], l40_[i]);
        l60_[i] = latch(own, r60_[i], l60_[i]);
        l80_[i] = latch(own, r80_[i], l80_[i]);
        le0_[i] = latch(own, re0_[i], le0_[i]);
        lfnum_[i] = latch(own, ch_fnum_[i], lfnum_[i]);
        lblock_[i] = latch(own, ch_block_[i], lblock_[i]);
        lkey_[i] = latch(own, ch_key_[i], lkey_[i]);
        lpan_[i] = latch(own, ch_pan_[i], lpan_[i]);
        lcon_[i] = latch(channel_first, ch_con_[i], lcon_[i]);
        lfirst_con_[i] = latch(chain_first, first_con_[i], lfirst_con_[i]);
        lsecond_con_[i] = latch(chain_first, second_con_[i], lsecond_con_[i]);
        lfb_[i] = latch(since + s + reads_feedback >= 0, ch_fb_[i], lfb_[i]);
        lnts_[i] = latch(chip, nts_, lnts_[i]);
        ldrum_keys_[i] = latch(chip, drum_keys_, ldrum_keys_[i]);
        ldam_[i] = latch(chip, dam_, ldam_[i]);
        ldvb_[i] = latch(chip, dvb_, ldvb_[i]);
        lrhythm_[i] = latch(chip, rhythm_, lrhythm_[i]);
        lrhythm_keys_[i] = latch(since + rhythm_keys_time >= 0, rhythm_, lrhythm_keys_[i]);
        lrhythm_before_[i] = latch(since + s + rhythm_before >= 0, rhythm_, lrhythm_before_[i]);
        lrhythm_after_[i] = latch(since + s + rhythm_after >= 0, rhythm_, lrhythm_after_[i]);
        lfour_op_[i] = latch(since + chain_read_time[i] >= 0, four_op_, lfour_op_[i]);
    }
    refresh_routing();
    derive();
}

void Core::refresh_routing()
{
    OPL3_VECTOR_LOOP
    for (std::size_t i = 0; i < lanes; ++i) {
        const int s = lane_slot_[i];
        const bool first = lane_first_[i] != 0, second_channel = lane_second_channel_[i] != 0;
        const bool chain = (lfour_op_[i] & chain_bit[i]) != 0;
        const bool con = lcon_[i] != 0, first_con = lfirst_con_[i] != 0, second_con = lsecond_con_[i] != 0;
        const bool rhythm_early = lrhythm_before_[i] != 0, rhythm = lrhythm_[i] != 0, rhythm_late = lrhythm_after_[i] != 0;
        const int phase = drum_phase[i];
        const bool heard = s >= 0;
        // Two operators: what the channel's first slot read of its
        // connection decides both whether that slot is heard and whether it
        // modulates the second.
        const int second_modulation = con ? 0 : 1;
        int modulation = first ? 2 : second_modulation;
        int weight = !first || con ? 1 : 0;
        // Four operators: the chain A B C D of channels f and f + 3. B is
        // heard and C goes unmodulated when the chain adds A and B to C and
        // D (the first channel's connection 0, the second's 1, both as A read
        // them); C is heard and D goes unmodulated, as for two operators, when
        // the second channel's connection as C read it is 1, but not then. A
        // goes as for two operators, and so does B's modulation, and D is
        // heard.
        const bool two_pairs = !first_con && second_con;
        const bool c_heard = !two_pairs && con;
        const bool b = chain && !second_channel && !first, c = chain && second_channel && first;
        const bool d = chain && second_channel && !first;
        const int b_weight = two_pairs ? 1 : 0, c_weight = c_heard ? 1 : 0;
        const int c_modulation = two_pairs ? 0 : 1, d_modulation = c_heard ? 0 : 1;
        weight = b ? b_weight : weight;
        weight = c ? c_weight : weight;
        modulation = c ? c_modulation : modulation;
        modulation = d ? d_modulation : modulation;
        // Rhythm: channels 6, 7 and 8 of the first bank, slots 12 to 17. The
        // bass drum is the pair of channel 6 as it is, heard from its carrier
        // alone; the other four drums are a slot each, with neither feedback
        // nor modulation. A drum is heard twice as loud, and the values of the
        // hi-hat's and the tom's slots are not stored for feedback.
        const bool drum = s >= 12 && s < 18, alone = drum && s != 12 && s != 15;
        const int drum_weight = s == 12 ? 0 : 2;
        modulation = alone && rhythm_early ? 0 : modulation;
        weight = drum && rhythm_late ? drum_weight : weight;
        weight = heard ? weight : 0;
        modulation_[i] = static_cast<std::uint8_t>(modulation);
        weight_[i] = static_cast<std::uint8_t>(weight);
        hold_history_[i] = rhythm_late && (s == 13 || s == 14) ? 1 : 0;
        rhythm_phase_[i] = static_cast<std::uint8_t>(rhythm ? phase : 0);
    }
}

// What the passes go by, from what the slots read and the routing.
void Core::derive()
{
    OPL3_VECTOR_LOOP
    for (std::size_t lane = 0; lane < lanes; ++lane) {
        const int s = lane_slot_[lane];
        const unsigned r20 = l20_[lane], r40 = l40_[lane], r60 = l60_[lane], r80 = l80_[lane];
        const unsigned fnum = lfnum_[lane], block = lblock_[lane];
        const bool key = lkey_[lane] != 0, rhythm_keys = lrhythm_keys_[lane] != 0, dam = ldam_[lane] != 0;
        const std::int32_t drum_keys = drum_key_bit[lane] & ldrum_keys_[lane];
        const unsigned ksr = ymf262::key_scale(fnum, block, lnts_[lane] != 0, ((r20 >> 4) & 1) != 0);
        const unsigned ar = r60 >> 4, dr = r60 & 15, rr = r80 & 15, egt = (r20 >> 5) & 1;
        const unsigned by_state[4] = {ar, dr, egt != 0 ? 0u : rr, rr};
        std::uint32_t rates = 0;
        for (unsigned st = 0; st < 4; ++st)
            rates |= (ymf262::scaled_rate(by_state[st], ksr) | (by_state[st] != 0 ? 16u : 0u)) << (8 * st);
        rates_[lane] = rates;
        key_scale_[lane] = static_cast<std::int32_t>(ksr);
        key_scale_low_[lane] = static_cast<std::int32_t>(ksr & 3);
        const unsigned sl = r80 >> 4;
        const auto sustain = static_cast<std::int32_t>(sl);
        sustain_[lane] = sl == 15 ? 31 : sustain;
        key_on_[lane] = key || (rhythm_keys && drum_keys != 0) ? 1 : 0;
        level_offset_[lane] = static_cast<std::int32_t>(ymf262::level_offset(fnum, block, r40 >> 6, r40 & 63));
        const std::int32_t depth = dam ? 2 : 1;
        tremolo_depth_[lane] = (r20 & 0x80) != 0 ? depth : 0;
        fnum_v_[lane] = fnum;
        block_v_[lane] = block;
        multiple_v_[lane] = static_cast<std::uint32_t>(ymf262::multiple[r20 & 15]);
        vibrato_[lane] = (r20 >> 6) & 1;
        vibrato_depth_[lane] = ldvb_[lane];
        const ymf262::Wave wave = ymf262::wave(le0_[lane]);
        phase_shift_[lane] = wave.phase_shift;
        turn_bit_[lane] = wave.turn_bit;
        mute_bit_[lane] = wave.mute_bit;
        sign_bit_[lane] = wave.sign_bit;
        square_[lane] = wave.square;
        sawtooth_[lane] = wave.sawtooth;
        const unsigned fb = lfb_[lane];
        const std::int32_t by = modulation_[lane];
        modulated_by_[lane] = by == 2 && fb == 0 ? 0 : by;
        const std::int32_t shift = 9 - static_cast<std::int32_t>(fb);
        feedback_shift_[lane] = fb != 0 ? shift : 0;
        const std::int32_t w = weight_[lane];
        const unsigned pan = lpan_[lane];
        const bool on_a = (pan & 1) != 0, on_b = (pan & 2) != 0;
        a_now_[lane] = (on_a && s < first_late_on_a) ? w : 0;
        a_late_[lane] = (on_a && s >= first_late_on_a) ? w : 0;
        b_now_[lane] = (on_b && s < first_late_on_b) ? w : 0;
        b_late_[lane] = (on_b && s >= first_late_on_b) ? w : 0;
    }
    derive_soft_pan();
}

void Core::derive_soft_pan()
{
    OPL3_VECTOR_LOOP
    for (std::size_t i = 0; i < lanes; ++i) {
        soft_a_now_[i] = a_now_[i] * gain_a_[i];
        soft_a_late_[i] = a_late_[i] * gain_a_[i];
        soft_b_now_[i] = b_now_[i] * gain_b_[i];
        soft_b_late_[i] = b_late_[i] * gain_b_[i];
    }
}

// The envelope and the phase of every lane, for this pass, in two loops, so
// that neither holds more than a vector's registers can.
void Core::envelope_and_phase()
{
    const ymf262::Envelope_timing timing = counters_.envelope_timing();
    const std::int32_t tremolo_level = counters_.tremolo_level();
    const std::int32_t tremolo_shallow = tremolo_level >> 4, tremolo_deep = tremolo_level >> 2;
    OPL3_VECTOR_LOOP
    for (std::size_t i = 0; i < lanes; ++i) {
        const ymf262::Envelope_step e = ymf262::envelope(state_[i], level_[i], key_on_[i], rates_[i], key_scale_[i],
                                                         key_scale_low_[i], sustain_[i], timing);
        const std::int32_t depth = tremolo_depth_[i];
        const std::int32_t shallow = depth == 1 ? tremolo_shallow : 0;
        const std::int32_t tremolo = depth == 2 ? tremolo_deep : shallow;
        eg_out_[i] = ymf262::attenuation(level_[i], level_offset_[i] + tremolo);
        level_[i] = e.level;
        state_[i] = e.state;
        restart_[i] = e.restart ? 1 : 0;
    }
    const ymf262::Vibrato vibrato = ymf262::vibrato(counters_.vibrato_position);
    OPL3_VECTOR_LOOP
    for (std::size_t i = 0; i < lanes; ++i) {
        const std::uint32_t increment = ymf262::phase_increment(fnum_v_[i], block_v_[i], multiple_v_[i], vibrato_[i] != 0,
                                                                vibrato_depth_[i] != 0, vibrato);
        phase_out_[i] = ymf262::phase_out(phase_[i]);
        phase_[i] = ymf262::next_phase(phase_[i], increment, restart_[i] != 0);
    }
}

// Rhythm: the hi-hat, the snare and the top cymbal take phases of their own.
// The hi-hat's bits are taken every pass, the top cymbal's only in rhythm
// mode.
void Core::rhythm_phases()
{
    rhythm_bits_.take_hi_hat(phase_out_[lane_13]);
    if (rhythm_phase_[lane_13] != 0)
        phase_out_[lane_13] = rhythm_bits_.hi_hat(ymf262::hi_hat_noise(noise_));
    if (rhythm_phase_[lane_16] != 0)
        phase_out_[lane_16] = rhythm_bits_.snare(ymf262::snare_noise(noise_));
    if (rhythm_phase_[lane_17] != 0) {
        rhythm_bits_.take_top_cymbal(phase_out_[lane_17]);
        phase_out_[lane_17] = rhythm_bits_.top_cymbal();
    }
}

// The operators of one stage, over its lanes.
template <int Begin, int Before, int Width>
void Core::stage()
{
    OPL3_VECTOR_LOOP
    for (int j = 0; j < Width; ++j) {
        const int lane = Begin + j, source = Before + j;
        const auto i = static_cast<std::size_t>(lane);
        const std::int32_t mod = ymf262::modulation(out_[static_cast<std::size_t>(source)], out_1_[i] + out_2_[i],
                                                    feedback_shift_[i], modulated_by_[i]);
        out_[i] = ymf262::operator_output(phase_out_[i] + mod, phase_shift_[i], turn_bit_[i], mute_bit_[i],
                                          sign_bit_[i], square_[i], sawtooth_[i], eg_out_[i]);
    }
}

void Core::operators()
{
    // The feedback history of slots 13 and 31 and of 14 and 32, from their
    // queues: slot m reads the second and the fourth; slot m + 18 reads them
    // after m's value has gone in, or else the same two.
    for (std::size_t q = 0; q < 2; ++q) {
        const std::array<std::int32_t, 4> &f = shared_history_[q];
        const std::size_t low = q == 0 ? lane_13 : lane_14, high = q == 0 ? lane_31 : lane_32;
        out_1_[low] = f[1];
        out_2_[low] = f[3];
        const bool stored = hold_history_[low] == 0;
        out_1_[high] = stored ? f[0] : f[1];
        out_2_[high] = stored ? f[2] : f[3];
    }
    stage<stage_begin[0], stage_begin[0], stage_width[0]>();
    stage<stage_begin[1], stage_begin[0], stage_width[1]>();
    stage<stage_begin[2], stage_begin[1], stage_width[2]>();
    stage<stage_begin[3], stage_begin[2], stage_width[3]>();
}

void Core::store_history()
{
    OPL3_VECTOR_LOOP
    for (std::size_t i = 0; i < lanes; ++i) {
        out_2_[i] = out_1_[i];
        out_1_[i] = out_[i];
    }
    for (std::size_t q = 0; q < 2; ++q) {
        std::array<std::int32_t, 4> &f = shared_history_[q];
        const std::size_t low = q == 0 ? lane_13 : lane_14, high = q == 0 ? lane_31 : lane_32;
        if (hold_history_[low] == 0)
            f = {out_[low], f[0], f[1], f[2]};
        f = {out_[high], f[0], f[1], f[2]};
    }
}

void Core::accumulate(std::int32_t *frame)
{
    // Nineteen bits, as the chip adds, then sixteen.
    const auto clip = [](std::int32_t x) {
        x = static_cast<std::int32_t>(static_cast<std::uint32_t>(x) << 13) >> 13;
        return x > 32767 ? 32767 : (x < -32768 ? -32768 : x);
    };
    if (!soft_pan_) {
        std::int32_t a_now = 0, a_late = 0, b_now = 0, b_late = 0;
        OPL3_VECTOR_LOOP
        for (std::size_t i = 0; i < lanes; ++i) {
            const std::int32_t v = out_[i];
            a_now += v * a_now_[i];
            a_late += v * a_late_[i];
            b_now += v * b_now_[i];
            b_late += v * b_late_[i];
        }
        frame[0] = clip(a_now + a_carry_);
        frame[1] = clip(b_now + b_carry_);
        a_carry_ = a_late;
        b_carry_ = b_late;
        a_carry_soft_ = std::int64_t{a_late} * 65536;
        b_carry_soft_ = std::int64_t{b_late} * 65536;
        return;
    }
    // Soft panning: the gains in sixteenths of sixteen bits, the sums exact,
    // and one rounding (half away from zero) at the end.
    std::int64_t a_now = 0, a_late = 0, b_now = 0, b_late = 0;
    OPL3_VECTOR_LOOP
    for (std::size_t i = 0; i < lanes; ++i) {
        const std::int64_t v = out_[i];
        a_now += v * soft_a_now_[i];
        a_late += v * soft_a_late_[i];
        b_now += v * soft_b_now_[i];
        b_late += v * soft_b_late_[i];
    }
    const auto round16 = [](std::int64_t x) {
        return static_cast<std::int32_t>(x >= 0 ? (x + 32768) >> 16 : -((32768 - x) >> 16));
    };
    frame[0] = clip(round16(a_now + a_carry_soft_));
    frame[1] = clip(round16(b_now + b_carry_soft_));
    a_carry_soft_ = a_late;
    b_carry_soft_ = b_late;
    a_carry_ = round16(a_late);
    b_carry_ = round16(b_late);
}

void Core::generate(std::int32_t *frame)
{
    // The write that takes effect in this pass: one the last sample sent into
    // it, or one this sample sends so early that it still reaches it (the
    // writes are 80 slot times apart, so never both).
    Landing due = landing_next_, now;
    landing_next_ = Landing{};
    schedule_writes(now, landing_next_);
    if (now.due)
        due = now;

    // What the slots read of the last write that they had not read yet.
    if (landed_age_ >= 0) {
        relatch();
        if (36 * landed_age_ + reads_feedback >= landed_slot_time_)
            landed_age_ = -1;
    }
    if (due.due) {
        land(due);
        relatch();
    }

    envelope_and_phase();
    rhythm_phases();
    operators();
    store_history();
    accumulate(frame);
    counters_.advance();
    noise_ = ymf262::noise_pass(noise_);
    if (landed_age_ >= 0)
        ++landed_age_;
}

}  // namespace adlplug::opl3
