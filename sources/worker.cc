//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// SPDX-FileCopyrightText: 2018-2019 Jean Pierre Cimalando
// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: BSL-1.0 AND GPL-3.0-or-later
//
// This file comes from ADLplug and was modified for ADLplug-Next. The notice at
// the top is ADLplug's; the LICENSE it names was ADLplug's copy of the Boost
// Software License, now LICENSES/BSL-1.0.txt. The SPDX lines name the copyright
// holders and licenses in the machine-readable form of the REUSE specification:
// ADLplug's code is under the Boost Software License 1.0, and ADLplug-Next's
// changes are under the GNU General Public License, version 3 or any later
// version (LICENSES/GPL-3.0-or-later.txt).

#include "worker.h"
#include "plugin_processor.h"
#include "parameter_block.h"
#include "adl/measurer.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <limits>
#include <system_error>

#if 1
#   define trace(fmt, ...) ((void)0)
#else
#   define trace(fmt, ...) std::fprintf(stderr, "[Worker] " fmt "\n" __VA_OPT__(,) __VA_ARGS__)
#endif

namespace {

template <class To, class From>
constexpr To saturate_to(From value) noexcept
{
    const auto max = static_cast<From>(std::numeric_limits<To>::max());
    return static_cast<To>(std::clamp<From>(value, From{0}, max));
}

}  // namespace

Worker::Worker(AdlplugAudioProcessor &proc)
    : proc_(proc)
{
}

Worker::~Worker()
{
    // Joining a thread can fail, and std::thread::join says so by throwing. A
    // destructor that lets that out ends the program where a plugin is only being
    // taken away, and a caller has nothing it could do with the news: the thread
    // it would hear about is gone with the object. So the word stops here. Where
    // the worker is stopped on purpose, stop_worker() is called directly and
    // throws as it always did.
    try {
        stop_worker();
    }
    // Nothing is done with what is caught, which is the point of catching it.
    // NOLINTNEXTLINE(bugprone-empty-catch)
    catch (...) {
    }
}

void Worker::start_worker()
{
    stop_worker();
    quit_.store(false);
    thread_ = std::thread([this] { run(); });
}

void Worker::stop_worker()
{
    if (thread_.joinable()) {
        quit_.store(true);
        sem_.post();
        thread_.join();
    }
}

void Worker::run()
{
    const AdlplugAudioProcessor &proc = proc_;
    Semaphore &sem = sem_;

    Simple_Fifo &mq_recv = proc.message_queue_to_worker();
    Simple_Fifo &mq_send = proc.message_queue_for_worker();

    const auto receive_one = [&] {
        const Buffered_Message msg = Messages::read(mq_recv);
        assert(msg);
        handle_message(msg);
        Messages::finish_read(mq_recv, msg);
    };

    // Handles the messages that have already been announced; false once the
    // worker is asked to quit.
    const auto receive_pending = [&] {
        while (sem.try_wait()) {
            if (quit_.load())
                return false;
            receive_one();
        }
        return true;
    };

    trace("Start");

    for (;;) {
        sem.wait();
        if (quit_.load())
            break;
        receive_one();
        if (!receive_pending())
            break;

        // Measure everything requested so far, picking up new messages
        // between measurements. (This used to measure a single instrument
        // per wake-up and leave the rest until the next message arrived.)
        bool quit = false;
        while (!quit && !measure_requests_.empty()) {
            Buffered_Message msg = Messages::write<Messages::Worker::MeasurementResult>(mq_send);
            while (!msg) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                if (!receive_pending()) {
                    quit = true;
                    break;
                }
                msg = Messages::write<Messages::Worker::MeasurementResult>(mq_send);
            }
            if (quit)
                break;

            const auto it = measure_requests_.begin();
            Messages::Worker::MeasurementResult result;
            measure(it->first, it->second, result);
            Messages::set_body(msg, result);
            Messages::finish_write(mq_send, msg);
            measure_requests_.erase(it);

            quit = !receive_pending();
        }
        if (quit)
            break;
    }

    trace("Stop");
}

void Worker::handle_message(const Buffered_Message &msg)
{
    AdlplugAudioProcessor &proc = proc_;

    switch (static_cast<Fx_Message>(msg.header.tag)) {
    case Fx_Message::RequestMeasurement: {
        const auto body = Messages::body<Messages::Fx::RequestMeasurement>(msg);
        const Bank_Id id = body.bank;
        const unsigned program = body.program;
        trace("Measurement requested for %c%u:%u:%u",
              id.percussive ? 'P' : 'M', id.msb, id.lsb, program);
        const std::uint32_t full_id = (id.to_integer() << 7) | program;
        measure_requests_[full_id] = body.instrument;
        break;
    }
    case Fx_Message::RequestChipSettings: {
        const auto body = Messages::body<Messages::Fx::RequestChipSettings>(msg);
        trace("Chip settings requested");
        const std::unique_lock<std::mutex> lock = proc.acquire_player_nonrt();
        proc.set_chip_settings_nonrt(body.cs);
        proc.mark_for_notification(Cb_ChipSettings);
        break;
    }
    default:
        assert(false);
        break;
    }
}

void Worker::measure(std::uint32_t full_id, const Instrument &ins, Messages::Worker::MeasurementResult &body)
{
    const Bank_Id id = Bank_Id::from_integer(full_id >> 7);
    const unsigned program = full_id & 127;

    trace("Measuring for %c%u:%u:%u",
          id.percussive ? 'P' : 'M', id.msb, id.lsb, program);

    Measurer::DurationInfo result {};
    Measurer::ComputeDurations(ins, result);

    trace("Finished measuring %c%u:%u:%u: %llu ms on, %llu ms off",
          id.percussive ? 'P' : 'M', id.msb, id.lsb, program,
          static_cast<unsigned long long>(result.ms_sound_kon),
          static_cast<unsigned long long>(result.ms_sound_koff));

    body.bank = id;
    body.program = static_cast<std::uint8_t>(program);
    body.instrument = ins;
    body.ms_sound_kon = saturate_to<std::uint16_t>(result.ms_sound_kon);
    body.ms_sound_koff = saturate_to<std::uint16_t>(result.ms_sound_koff);
}
