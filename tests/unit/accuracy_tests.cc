// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// Tests of the arithmetic between the chip and the host, which is binary64 all
// the way: what comes out has to be as close to the exact result as binary64
// guarantees, whatever the compiler made of the arithmetic.
//
// The chip gives whole numbers. The library makes each one a double, n times
// 1/32767; the resampler filters at the host's rate, each sample a sum of
// products of the filter's coefficients and the chip's samples; and the
// processor multiplies by the output gain and takes the DC out with a filter of
// one pole (AdlplugAudioProcessor::process). How a build does that arithmetic is
// the compiler's to choose within the rules: it adds the products in the order
// its vectors want (mp::transform::dot lets it), fuses a multiply with the add
// after it where the processor has the instruction, and the last bits differ
// from one build to the next. No build can come out further from the exact
// result than the arithmetic allows, and that is what is checked here, sample
// by sample, rather than the bits.
//
// **The bound.** An operation of binary64, rounding to nearest, gives the exact
// result times 1 + d with |d| at most u = 2^-53, give or take 2^-1022 where the
// result is so small that the processor flushes it to zero (Higham, Accuracy
// and Stability of Numerical Algorithms, 2nd ed., chapter 2). A fused
// multiply-add is one such operation, so a sum of n products, added in any order
// and fused or not, is within gamma_n = nu/(1 - nu) times the sum of their
// magnitudes (chapter 3), and an operation on values that are off already
// carries what they are off by. Followed sample by sample, that bounds every
// sample of the path: the conversion is two roundings from n/32767, a sample of
// the filter is gamma_n of its taps from the sum of their products, and the DC
// filter adds its roundings at each sample to what its pole carries forward
// from the samples before. The exact result is worked out alongside in
// double-double arithmetic, two doubles to a number and 106 bits, whose
// operations are bounded in the same way at a few 2^-106 each (Joldes, Muller
// and Popescu, Tight and rigorous error bounds for basic building blocks of
// double-word arithmetic, ACM Transactions on Mathematical Software 44(2),
// 2017). A unit of 2^-100 is taken for them, which is room enough, and the
// reference gets a bound of its own. A sample passes when it is within the two
// bounds of the reference. The bounds are worked out in binary64 as well, out of
// positive numbers, in chains of a few hundred thousand roundings at most here,
// which can make one smaller than it is by a part in 2^30; they are taken a part
// in 2^20 larger.
//
// **That the check sees binary32.** A bound that let anything through would be
// passed as well. The same samples rounded once to binary32, which rounds at
// 2^-24, have to fail it, and they fail it by orders of magnitude: the bounds
// come to some tens or thousands of u of the signal.

#include "test.h"
#include "adl/chip_settings.h"
#include "adl/player.h"
#include "definitions.h"
#include "dsp/dc_filter.h"
#include "resampling_settings.h"
#include "resources.h"
#include "utility/chip_resampler.h"
#include "utility/pak.h"
#include "JuceHeader.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

namespace {

constexpr double u = 0x1p-53;       // binary64's unit roundoff
constexpr double u_dd = 0x1p-100;   // what each double-double operation here is held to
constexpr double eta = 0x1p-1022;   // the most a flush to zero takes away
constexpr double slack = 1.0 + 0x1p-20;

// The most n roundings at the unit given can take a value away from where it
// was, as a fraction of it.
[[nodiscard]] double gamma_n(double n, double unit)
{
    return n * unit / (1.0 - n * unit);
}

// A number as the sum of two doubles, the second no more than half an ulp of the
// first.
struct Double_Double {
    double hi = 0.0;
    double lo = 0.0;
};

[[nodiscard]] double magnitude(Double_Double x)
{
    return std::abs(x.hi) + std::abs(x.lo);
}

// a + b exactly (Knuth's TwoSum).
[[nodiscard]] Double_Double two_sum(double a, double b)
{
    const double s = a + b;
    const double b_part = s - a;
    const double a_part = s - b_part;
    return {.hi = s, .lo = (a - a_part) + (b - b_part)};
}

// a + b exactly, where |a| is at least |b| (Dekker's Fast2Sum).
[[nodiscard]] Double_Double fast_two_sum(double a, double b)
{
    const double s = a + b;
    return {.hi = s, .lo = b - (s - a)};
}

// a * b exactly: the fused multiply-add gives back what the product rounded off.
[[nodiscard]] Double_Double two_product(double a, double b)
{
    const double p = a * b;
    return {.hi = p, .lo = std::fma(a, b, -p)};
}

// AccurateDWPlusDW of Joldes, Muller and Popescu.
[[nodiscard]] Double_Double operator+(Double_Double x, Double_Double y)
{
    const Double_Double s = two_sum(x.hi, y.hi);
    const Double_Double t = two_sum(x.lo, y.lo);
    const Double_Double v = fast_two_sum(s.hi, s.lo + t.hi);
    return fast_two_sum(v.hi, t.lo + v.lo);
}

[[nodiscard]] Double_Double operator-(Double_Double x)
{
    return {.hi = -x.hi, .lo = -x.lo};
}

// DWTimesFP3.
[[nodiscard]] Double_Double operator*(Double_Double x, double y)
{
    const Double_Double c = two_product(x.hi, y);
    return fast_two_sum(c.hi, std::fma(x.lo, y, c.lo));
}

// DWDivFP1.
[[nodiscard]] Double_Double operator/(Double_Double x, double y)
{
    const double t = x.hi / y;
    const Double_Double back = two_product(t, y);
    const double rest = ((x.hi - back.hi) - back.lo) + x.lo;
    return fast_two_sum(t, rest / y);
}

// A sample of the path: its reference in double-double, how far that may be from
// the exact value, and how far the binary64 sample may be from the exact value.
struct Traced {
    Double_Double reference;
    double reference_error = 0.0;
    double error = 0.0;
};

// The most the exact value can be in magnitude.
[[nodiscard]] double size_of(const Traced &x)
{
    return magnitude(x.reference) + x.reference_error;
}

// The chip's samples as the library hands them over: n times 1/32767 in
// binary64, two roundings from n/32767 (adl_cvtReal and opn2_cvtReal). The
// whole number is read back from the sample: 32767 times the sample is within
// 3u|n| of n, far closer than the half that lies between one whole number and
// the next.
[[nodiscard]] std::vector<Traced> converted(const std::vector<double> &samples)
{
    std::vector<Traced> traced;
    traced.reserve(samples.size());
    for (const double sample : samples) {
        const auto n = static_cast<double>(std::llround(sample * 32767.0));
        const Double_Double exact = Double_Double{.hi = n} / 32767.0;
        const double size = magnitude(exact);
        traced.push_back({.reference = exact,
                          .reference_error = u_dd * size,
                          .error = gamma_n(2.0, u) * (size + u_dd * size)});
    }
    return traced;
}

// One stage of the resampler, from the definition of what it computes: the
// input with up - 1 zeros after each sample, filtered by the prototype, and
// every down-th sample of that from the centre on. Output k is the sum over the
// taps t that land on a sample of h[t] times the input's sample
// (k * down + centre - t) / up, the stream starting from silence. The stage
// adds the same products, in an order of its own, over taps_per_phase() terms.
[[nodiscard]] std::vector<Traced> resampled(const mp::resample::Resampler &stage, const std::vector<Traced> &in)
{
    const std::vector<double> &h = stage.prototype();
    const std::size_t up = stage.up();
    const std::size_t down = stage.down();
    const std::size_t length = h.size();
    const std::size_t centre = stage.aligned() ? (length - 1) / 2 : 0;
    const auto terms = static_cast<double>(stage.taps_per_phase());
    const double growth = gamma_n(terms, u);
    const double growth_dd = gamma_n(terms, u_dd);

    std::vector<Traced> out;
    out.reserve(in.size() * up / down + 1);
    for (std::size_t k = 0;; ++k) {
        const std::size_t at = k * down + centre;
        const std::size_t newest = at / up;
        if (newest >= in.size())
            break;  // the stage has not had that sample yet either
        const std::size_t oldest = at + 1 > length ? (at + 1 - length + up - 1) / up : 0;

        Double_Double sum;
        double reach = 0.0;       // the sum of |h| times what each binary64 sample can be
        double reach_dd = 0.0;    // the sum of |h| times each double-double sample
        double carried = 0.0;     // the sum of |h| times how far each binary64 sample may be off
        double carried_dd = 0.0;  // the sum of |h| times how far each double-double sample may be off
        for (std::size_t i = oldest; i <= newest; ++i) {
            const double tap = h[at - i * up];
            const Traced &x = in[i];
            sum = sum + x.reference * tap;
            const double weight = std::abs(tap);
            reach += weight * (size_of(x) + x.error);
            reach_dd += weight * magnitude(x.reference);
            carried += weight * x.error;
            carried_dd += weight * x.reference_error;
        }
        out.push_back({.reference = sum,
                       .reference_error = growth_dd * reach_dd + carried_dd,
                       .error = growth * reach + carried + 4.0 * terms * eta});
    }
    return out;
}

// A product by a constant that the two computations share, the binary64 one
// rounded once.
[[nodiscard]] Traced scaled(const Traced &x, double c)
{
    const double w = std::abs(c);
    return {.reference = x.reference * c,
            .reference_error = w * (x.reference_error + u_dd * magnitude(x.reference)),
            .error = w * x.error + u * w * (size_of(x) + x.error) + eta};
}

// The processor's output stage, from its definition: z = y * gain, then the DC
// filter, x = b0 * z and out = (x - x') + p * out' for the x and the out of the
// sample before, from silence (dsp/dc_filter.h). The compiler may fuse
// p * out' with the sum, or round it on its own first.
[[nodiscard]] std::vector<Traced> output_stage(const std::vector<Traced> &in, double gain, double b0, double p)
{
    const double w = std::abs(p);
    std::vector<Traced> out;
    out.reserve(in.size());
    Traced x_before;
    Traced out_before;
    for (const Traced &y : in) {
        const Traced x = scaled(scaled(y, gain), b0);

        Traced d;
        d.reference = x.reference + -x_before.reference;
        d.reference_error = x.reference_error + x_before.reference_error +
            u_dd * (magnitude(x.reference) + magnitude(x_before.reference));
        d.error = x.error + x_before.error + u * (size_of(d) + x.error + x_before.error) + eta;

        const Double_Double pole = out_before.reference * p;
        Traced o;
        o.reference = d.reference + pole;
        o.reference_error = d.reference_error +
            w * (out_before.reference_error + u_dd * magnitude(out_before.reference)) +
            u_dd * (magnitude(d.reference) + magnitude(pole));
        o.error = d.error + w * out_before.error +
            gamma_n(2.0, u) * (size_of(d) + d.error + w * (size_of(out_before) + out_before.error)) + 3.0 * eta;

        out.push_back(o);
        x_before = x;
        out_before = o;
    }
    return out;
}

// The farthest a sample is from its reference, as a fraction of how far it may
// be: above one, a sample is out of its bound.
[[nodiscard]] double worst(const std::vector<double> &samples, const std::vector<Traced> &traced)
{
    CHECK(traced.size() >= samples.size());
    const std::size_t count = std::min(samples.size(), traced.size());
    double ratio = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const double distance = magnitude(Double_Double{.hi = samples[i]} + -traced[i].reference);
        const double allowed = (traced[i].reference_error + traced[i].error) * slack;
        if (distance == 0.0)
            continue;
        if (allowed == 0.0)
            return std::numeric_limits<double>::infinity();
        ratio = std::max(ratio, distance / allowed);
    }
    return ratio;
}

// The samples as they would be had they been held in binary32 once.
[[nodiscard]] std::vector<double> through_binary32(const std::vector<double> &samples)
{
    std::vector<double> rounded;
    rounded.reserve(samples.size());
    for (const double sample : samples)
        rounded.push_back(static_cast<double>(static_cast<float>(sample)));
    return rounded;
}

// The MIDI of the test, by the millisecond it is sent at: four parts and the
// drums, with the programs tools/render plays, spread across the stereo field,
// with a bend, a modulation and a change of chord on the way.
struct Timed_Midi {
    unsigned ms = 0;
    std::uint8_t status = 0;
    std::uint8_t first = 0;
    std::uint8_t second = 0;
};

constexpr Timed_Midi midi_events[] {
    {.ms = 0, .status = 0xc0, .first = 0},
    {.ms = 0, .status = 0xc1, .first = 19},
    {.ms = 0, .status = 0xc2, .first = 33},
    {.ms = 0, .status = 0xc3, .first = 81},
    {.ms = 0, .status = 0xb1, .first = 10, .second = 20},
    {.ms = 0, .status = 0xb2, .first = 10, .second = 110},
    {.ms = 0, .status = 0xb3, .first = 10, .second = 40},
    {.ms = 0, .status = 0x90, .first = 60, .second = 100},
    {.ms = 0, .status = 0x90, .first = 64, .second = 100},
    {.ms = 0, .status = 0x90, .first = 67, .second = 100},
    {.ms = 0, .status = 0x91, .first = 57, .second = 90},
    {.ms = 0, .status = 0x92, .first = 40, .second = 110},
    {.ms = 0, .status = 0x93, .first = 72, .second = 80},
    {.ms = 0, .status = 0x99, .first = 36, .second = 120},
    {.ms = 60, .status = 0x99, .first = 42, .second = 90},
    {.ms = 100, .status = 0xe1, .first = 0, .second = 0x50},
    {.ms = 100, .status = 0x99, .first = 38, .second = 110},
    {.ms = 150, .status = 0x83, .first = 72},
    {.ms = 150, .status = 0x93, .first = 76, .second = 80},
    {.ms = 150, .status = 0x99, .first = 42, .second = 90},
    {.ms = 200, .status = 0x80, .first = 60},
    {.ms = 200, .status = 0x80, .first = 64},
    {.ms = 200, .status = 0x80, .first = 67},
    {.ms = 200, .status = 0x90, .first = 62, .second = 100},
    {.ms = 200, .status = 0x90, .first = 65, .second = 100},
    {.ms = 200, .status = 0x90, .first = 69, .second = 100},
    {.ms = 200, .status = 0xb3, .first = 1, .second = 100},
    {.ms = 200, .status = 0x99, .first = 36, .second = 120},
    {.ms = 250, .status = 0xb1, .first = 10, .second = 100},
    {.ms = 250, .status = 0x99, .first = 42, .second = 90},
};

void send(Player &pl, const Timed_Midi &event)
{
    const std::uint8_t message[3] {event.status, event.first, event.second};
    // A program change is the one message here of two bytes.
    pl.play_midi(message, (event.status & 0xf0) == 0xc0 ? 2u : 3u);
}

// A player as the processor makes one (create_player), with the default bank
// and the default chip settings, at the rate given.
bool set_up(Player &pl, unsigned rate, const Chip_Settings &settings)
{
    pl.init(rate);
    Pak_File_Reader pak;
    CHECK(pak.init_with_data(Res::banks_pak.data, Res::banks_pak.size));
    const std::vector<std::uint8_t> bank = pak.extract(0);
    CHECK(!bank.empty());
    if (bank.empty() || !pl.load_bank_data(bank.data(), bank.size()))
        return false;
    pl.set_soft_pan_enabled(true);
    // What set_player_chip_settings gives it.
    pl.set_emulator(settings.emulator);
    CHECK(pl.emulator() == settings.emulator);
    pl.set_num_chips(settings.chip_count);
    pl.set_channel_alloc_mode(settings.chan_alloc);
#if defined(ADLPLUG_OPL3)
    pl.set_num_4ops(settings.fourop_count);
#elif defined(ADLPLUG_OPN2)
    pl.set_chip_type(settings.chip_type);
#endif
    return true;
}

// The path at one host rate and with one way of resampling, as the processor
// takes it: the player at the chip's rate and the filter after it when the
// filter can be built, the player at the host's rate otherwise, then the output
// gain and the DC filter. Three tenths of a second of it, at the master volume
// of one, its default.
void check_path(unsigned host_rate, const Resampling_Settings &resampling)
{
    Chip_Settings settings;
    settings.emulator = get_emulator_defaults().default_index;
    const unsigned chip_rate = chip_sample_rate(settings);

    Chip_Resampler resampler;
    std::string why;
    const bool filtered = resampling.own_filter &&
        resampler.prepare(chip_rate, host_rate, midi_interval_max, resampling.design, why);

    Player pl;
    if (!set_up(pl, filtered ? chip_rate : host_rate, settings))
        return;

    const unsigned frames = host_rate * 3 / 10;
    std::vector<double> chip[2];
    std::vector<double> host[2] {std::vector<double>(frames), std::vector<double>(frames)};
    {
        const juce::ScopedNoDenormals no_denormals;
        std::size_t next_event = 0;
        for (unsigned done = 0; done < frames;) {
            const auto now_ms = static_cast<unsigned>(std::uint64_t{done} * 1000 / host_rate);
            for (; next_event < std::size(midi_events) && midi_events[next_event].ms <= now_ms; ++next_event)
                send(pl, midi_events[next_event]);
            const unsigned block = std::min(frames - done, midi_interval_max);
            resampler.pull(host[0].data() + done, host[1].data() + done, block,
                           [&pl, &chip](double *left, double *right, unsigned count) {
                               pl.generate(left, right, count, 1);
                               chip[0].insert(chip[0].end(), left, left + count);
                               chip[1].insert(chip[1].end(), right, right + count);
                           });
            done += block;
        }
    }

    const double gain = Player::output_gain();
    Dc_Filter filter;
    filter.cutoff(5.0 / static_cast<double>(host_rate));  // as prepareToPlay sets it
    std::vector<double> out[2] {std::vector<double>(frames), std::vector<double>(frames)};
    {
        const juce::ScopedNoDenormals no_denormals;
        for (std::size_t c = 0; c < 2; ++c) {
            Dc_Filter dc = filter;
            for (std::size_t i = 0; i < frames; ++i)
                out[c][i] = dc.process(host[c][i] * gain);
        }
    }

    // How far the samples are from their references at each stage, as fractions
    // of their bounds: the worst of the two channels as they are, and the least
    // bad of them once through binary32.
    double chip_worst = 0.0, filter_worst = 0.0, out_worst = 0.0;
    double chip_coarse = std::numeric_limits<double>::infinity();
    double filter_coarse = chip_coarse, out_coarse = chip_coarse;
    for (std::size_t c = 0; c < 2; ++c) {
        // Something to measure: most of the samples are not silence.
        const auto sounding = static_cast<std::size_t>(
            std::count_if(chip[c].begin(), chip[c].end(), [](double sample) { return sample != 0.0; }));
        CHECK(2 * sounding > chip[c].size());

        const std::vector<Traced> x = converted(chip[c]);
        chip_worst = std::max(chip_worst, worst(chip[c], x));
        chip_coarse = std::min(chip_coarse, worst(through_binary32(chip[c]), x));

        std::vector<Traced> y = x;
        if (filtered) {
            const mp::resample::Cascade &cascade = resampler.cascade();
            for (std::size_t s = 0; s < cascade.size(); ++s)
                y = resampled(cascade.stage(s), y);
            filter_worst = std::max(filter_worst, worst(host[c], y));
            filter_coarse = std::min(filter_coarse, worst(through_binary32(host[c]), y));
        }
        CHECK(y.size() >= frames);
        y.resize(std::min<std::size_t>(y.size(), frames));

        const std::vector<Traced> o = output_stage(y, gain, filter.b0_, filter.p_);
        out_worst = std::max(out_worst, worst(out[c], o));
        out_coarse = std::min(out_coarse, worst(through_binary32(out[c]), o));
    }

    CHECK(chip_worst <= 1.0);
    CHECK(out_worst <= 1.0);
    CHECK(chip_coarse > 1.0);
    CHECK(out_coarse > 1.0);
    std::string how = "interpolated by the library";
    if (filtered) {
        CHECK(filter_worst <= 1.0);
        CHECK(filter_coarse > 1.0);
        how = "filtered";
        const mp::resample::Cascade &cascade = resampler.cascade();
        for (std::size_t s = 0; s < cascade.size(); ++s) {
            const mp::resample::Resampler &stage = cascade.stage(s);
            how += " " + std::to_string(stage.up()) + "/" + std::to_string(stage.down()) + " at " +
                std::to_string(stage.taps_per_phase()) + " taps";
        }
        std::printf("%u Hz, %s: the chip's samples %.3g, the filter's %.3g, the output %.3g of their bounds; "
                    "through binary32 once, %.3g, %.3g and %.3g times them\n",
                    host_rate, how.c_str(), chip_worst, filter_worst, out_worst, chip_coarse, filter_coarse,
                    out_coarse);
    } else {
        if (resampling.own_filter)
            how += ", the filter refused";
        std::printf("%u Hz, %s: the chip's samples %.3g and the output %.3g of their bounds; "
                    "through binary32 once, %.3g and %.3g times them\n",
                    host_rate, how.c_str(), chip_worst, out_worst, chip_coarse, out_coarse);
    }
}

}  // namespace

// The output of each kind of path a build of the plugin takes, as close to the
// exact result as binary64 guarantees: OPL3 through its filter at host rates
// below and above the chip's, and at one through the library's interpolation;
// the OPN chips, whose rates are prime or nearly so and refuse the filter of the
// default setting, at that setting and with the fast one, which builds.
ADLPLUG_TEST(output_path_accuracy)
{
    const Resampling_Settings filter_of_default {};
    Resampling_Settings library;
    library.own_filter = false;
#if defined(ADLPLUG_OPL3)
    for (const unsigned rate : {44100u, 48000u, 96000u})
        check_path(rate, filter_of_default);
    check_path(44100, library);
#elif defined(ADLPLUG_OPN2)
    check_path(44100, filter_of_default);
    Resampling_Settings fast;
    CHECK(resampling_preset("fast", fast.design));
    check_path(48000, fast);
#endif
}

// The constants of the path that are worked out from a formula, each within the
// few roundings of binary64 that make it: a constant held in binary32 is 2^29
// of them out.
ADLPLUG_TEST(output_constants_accuracy)
{
    // The output gain, +3 dB, pow(10, 3/20). pow is not held to rounding
    // correctly, and the libraries of the systems the plugin is built on give
    // it within an ulp; 3/20 is rounded on the way in, which moves 10^x by less
    // than u of itself, ln 10 times 3/20 being 0.35. The reference is 10^(3/20)
    // to 106 bits.
    const double gain = Player::output_gain();
    constexpr Double_Double plus_3_db {.hi = 0x1.699c0f7e86e10p+0, .lo = -0x1.6f94fea43cb39p-54};
    const double ulp = std::nextafter(gain, 2.0) - gain;
    const double gain_off = magnitude(Double_Double{.hi = gain} + -plus_3_db);
    CHECK(gain_off <= (ulp + u * plus_3_db.hi) * slack);

    // The DC filter's coefficients, b0 = 1/(1 + w) and p = (1 - w)/(1 + w) for
    // w = pi * 5/rate (dsp/dc_filter.h, with the cutoff that prepareToPlay
    // gives it). b0 is two roundings from its exact value, those of 1 + w and of
    // the reciprocal, and the three that make w -- 5/rate, pi and their product
    // -- reach it scaled by w/(1 + w), a hundredth at most: within gamma_3. p
    // adds the roundings of 1 - w and of the product: within gamma_5. They are
    // checked as |1 - (1 + w) b0| and |(1 + w) p - (1 - w)|, which take no
    // division.
    constexpr Double_Double pi {.hi = 0x1.921fb54442d18p+1, .lo = 0x1.1a62633145c07p-53};
    constexpr Double_Double one {.hi = 1.0};
    double b0_worst = 0.0, p_worst = 0.0;
    for (const unsigned rate : {22050u, 44100u, 48000u, 96000u, 192000u}) {
        Dc_Filter filter;
        filter.cutoff(5.0 / static_cast<double>(rate));
        const Double_Double w = pi * 5.0 / static_cast<double>(rate);
        CHECK(w.hi <= 0.01);
        const Double_Double sum = one + w;
        const Double_Double difference = one + -w;
        const double b0_off = magnitude(one + -(sum * filter.b0_));
        const double p_off = magnitude(sum * filter.p_ + -difference);
        CHECK(b0_off <= gamma_n(3.0, u) * slack);
        CHECK(p_off <= gamma_n(5.0, u) * magnitude(difference) * slack);
        b0_worst = std::max(b0_worst, b0_off / u);
        p_worst = std::max(p_worst, p_off / (u * magnitude(difference)));
    }
    std::printf("the output gain %.3g ulp from 10^(3/20); the DC filter's b0 at most %.3g u and p %.3g u "
                "from theirs\n",
                gain_off / ulp, b0_worst, p_worst);
}
