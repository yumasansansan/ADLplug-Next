// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

#pragma once
#include "resample.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <string>
#include <vector>

// The chip runs at a rate of its own -- 49716 Hz for the OPL3, 53267 or 55466 for
// the OPN -- and a host asks for its own. Given the host's rate, the library runs
// the chip at the chip's and interpolates between the two linearly, which is two
// multiplies a frame and a top octave that sags and folds. This is the other way
// round: the chip's own samples come out of the library untouched, and a filter
// designed for that pair of rates does the resampling (thirdparty/MediaPerch,
// cmake/MediaPerch.cmake).
//
// What this adds to that filter is the bookkeeping a plugin needs. A host asks
// for exactly as many frames as it asks for, so this asks the chip for as many as
// that takes and holds what the filter produced beyond them until the next call.
// Nothing here allocates or waits once prepare() has returned, which is what lets
// pull() run on the audio thread (sources/utility/realtime.h); prepare() itself
// designs a filter and allocates for it, and belongs where the player is made.
class Chip_Resampler {
public:
    // Designs the filter for the pair of rates, and says why when it cannot.
    // A ratio that does not reduce -- 53267 is prime, so every host rate is
    // coprime with it -- needs more coefficients than the design will build at
    // the settings given, and the caller that hears no plays at the host's rate
    // instead and leaves the interpolating to the library, as before.
    //
    // Equal rates are not resampled at all: the filter for that is a unit
    // impulse, and the arithmetic to rediscover that per sample would be waste.
    [[nodiscard]] bool prepare(unsigned chip_rate, unsigned host_rate, unsigned max_frames,
                               const mp::resample::Design &design, std::string &why)
    {
        unprepare();
        if (chip_rate == 0 || host_rate == 0 || max_frames == 0) {
            why = "a rate of zero is not a rate";
            return false;
        }
        if (chip_rate == host_rate)
            return false;  // nothing to do, and nothing to say about it

        // The ratio, which says how many of the chip's frames a block of the
        // host's takes. The filter is asked for the same number, so it has to
        // know it before it is built.
        const unsigned common = std::gcd(chip_rate, host_rate);
        const std::uint64_t up = host_rate / common;
        const std::uint64_t down = chip_rate / common;
        const auto needed = static_cast<std::uint64_t>(max_frames) * down;
        max_in_ = static_cast<unsigned>((needed + up - 1) / up) + 2;

        if (!cascade_.configure(chip_rate, host_rate, 2, max_in_, design, why)) {
            unprepare();
            return false;
        }

        const auto room = cascade_.max_output(max_in_);
        for (std::size_t channel = 0; channel < 2; ++channel) {
            chip_float_[channel].assign(max_in_, 0.0f);
            chip_[channel].assign(max_in_, 0.0);
            host_[channel].assign(room, 0.0);
            held_[channel].assign(room, 0.0);
            chip_planes_[channel] = chip_[channel].data();
            host_planes_[channel] = host_[channel].data();
        }
        held_count_ = 0;
        chip_rate_ = chip_rate;
        host_rate_ = host_rate;
        active_ = true;
        return true;
    }

    // There is no filter until prepare() says so. A player that starts again is
    // prepared again with it, which is where the stream starts from nothing: a
    // cascade has no way to forget a stream without designing it again, and the
    // one place that wants it forgotten is the one place that has just made a
    // player to go with it.
    void unprepare() noexcept
    {
        active_ = false;
        held_count_ = 0;
        held_at_ = 0;
        chip_rate_ = 0;
        host_rate_ = 0;
        max_in_ = 0;
    }

    [[nodiscard]] bool active() const noexcept { return active_; }
    [[nodiscard]] unsigned chip_rate() const noexcept { return chip_rate_; }
    [[nodiscard]] unsigned host_rate() const noexcept { return host_rate_; }
    // Per frame of the host's, per channel, as the filter counts them.
    [[nodiscard]] double multiplies() const noexcept { return active_ ? cascade_.multiplies() : 0.0; }
    // The coefficients the filter holds across its stages, which is the memory it
    // takes: one phase of the prototype per step of the ratio, each phase as long
    // as the loop that reads it.
    [[nodiscard]] std::uint64_t coefficients() const noexcept
    {
        if (!active_)
            return 0;
        std::uint64_t count = 0;
        for (std::size_t i = 0; i < cascade_.size(); ++i)
            count += static_cast<std::uint64_t>(cascade_.stage(i).up()) * cascade_.stage(i).taps_per_phase();
        return count;
    }
    // What the design achieved, measured by the design itself.
    [[nodiscard]] mp::resample::Response response() const noexcept
        { return active_ ? cascade_.response() : mp::resample::Response{}; }
    // What is left for the host to make up for: zero for a linear-phase filter,
    // whose delay is taken out where the output is sampled from.
    [[nodiscard]] double latency_frames() const noexcept { return active_ ? cascade_.latency_frames() : 0.0; }

    // Writes `frames` frames at the host's rate, asking `generate(left, right, n)`
    // for the chip's as it needs them. Without a filter it is the generator's
    // output as it stands.
    template <class Generate>
    void pull(float *left, float *right, unsigned frames, Generate &&generate)
    {
        if (!active_) {
            generate(left, right, frames);
            return;
        }

        unsigned done = take_held(left, right, frames);
        while (done < frames) {
            const unsigned want = std::min(max_in_, chip_frames_for(frames - done));
            generate(chip_float_[0].data(), chip_float_[1].data(), want);
            for (std::size_t channel = 0; channel < 2; ++channel)
                for (unsigned i = 0; i < want; ++i)
                    chip_[channel][i] = static_cast<double>(chip_float_[channel][i]);

            std::uint32_t produced = 0;
            const auto room = static_cast<std::uint32_t>(host_[0].size());
            if (!cascade_.process(chip_planes_, want, host_planes_, room, produced))
                produced = 0;  // only when asked to write past `room`, which is what max_output said it needs

            const unsigned use = std::min(static_cast<unsigned>(produced), frames - done);
            for (unsigned i = 0; i < use; ++i) {
                left[done + i] = static_cast<float>(host_[0][i]);
                right[done + i] = static_cast<float>(host_[1][i]);
            }
            done += use;

            // What the filter produced past this block waits for the next one.
            // The store is empty here: it was emptied above, and a block that
            // does not fill leaves nothing behind.
            held_count_ = static_cast<unsigned>(produced) - use;
            held_at_ = 0;
            for (std::size_t channel = 0; channel < 2; ++channel)
                for (unsigned i = 0; i < held_count_; ++i)
                    held_[channel][i] = host_[channel][use + i];
        }
    }

private:
    // How many of the chip's frames it takes to produce `frames` of the host's,
    // with one spare: the filter answers with what it can, and a call that comes
    // up short is asked again rather than trusted to a formula.
    [[nodiscard]] unsigned chip_frames_for(unsigned frames) const noexcept
    {
        const auto up = static_cast<std::uint64_t>(cascade_.up());
        const auto down = static_cast<std::uint64_t>(cascade_.down());
        const std::uint64_t needed = static_cast<std::uint64_t>(frames) * down;
        return static_cast<unsigned>((needed + up - 1) / up) + 1;
    }

    // The frames held from the last call, as many of them as fit.
    unsigned take_held(float *left, float *right, unsigned frames) noexcept
    {
        const unsigned use = std::min(held_count_, frames);
        for (unsigned i = 0; i < use; ++i) {
            left[i] = static_cast<float>(held_[0][held_at_ + i]);
            right[i] = static_cast<float>(held_[1][held_at_ + i]);
        }
        held_at_ += use;
        held_count_ -= use;
        return use;
    }

    mp::resample::Cascade cascade_;
    bool active_ = false;
    unsigned chip_rate_ = 0;
    unsigned host_rate_ = 0;
    unsigned max_in_ = 0;

    std::vector<float> chip_float_[2];
    std::vector<double> chip_[2];
    std::vector<double> host_[2];
    std::vector<double> held_[2];
    const double *chip_planes_[2] {};
    double *host_planes_[2] {};
    unsigned held_count_ = 0;
    unsigned held_at_ = 0;
};
