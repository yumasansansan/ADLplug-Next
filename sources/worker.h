//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

#pragma once
#include "JuceHeader.h"
#include "messages.h"
#include "adl/instrument.h"
#include "utility/semaphore.h"
#include <atomic>
#include <cstdint>
#include <thread>
#include <unordered_map>
class AdlplugAudioProcessor;

// Runs the jobs that are too slow for the audio thread: measuring how long
// instruments sound, and reconfiguring the chips. The processor sends one
// message per job and posts the semaphore once per message.
class Worker {
public:
    explicit Worker(AdlplugAudioProcessor &proc);
    ~Worker();

    Worker(const Worker &) = delete;
    Worker &operator=(const Worker &) = delete;

    void start_worker();
    void stop_worker();

    void postSemaphore()
        { sem_.post(); }

private:
    void run();
    void handle_message(const Buffered_Message &msg);
    static void measure(std::uint32_t full_id, const Instrument &ins, Messages::Worker::MeasurementResult &body);

    AdlplugAudioProcessor &proc_;
    std::thread thread_;
    std::atomic<bool> quit_ {false};
    Semaphore sem_;
    std::unordered_map<std::uint32_t, Instrument> measure_requests_;
};
