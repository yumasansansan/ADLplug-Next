//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// SPDX-FileCopyrightText: 2018 Jean Pierre Cimalando
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
