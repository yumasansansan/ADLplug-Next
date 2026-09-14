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
#if defined(__APPLE__)
#include <mach/mach.h>
#elif defined(_WIN32)
#include <climits>
#include <windows.h>
#else
#include <cerrno>
#include <semaphore.h>
#endif
#include <stdexcept>

// A counting semaphore. post() only makes a system call and never blocks, so
// the audio thread may use it to wake a worker.
class Semaphore {
public:
    explicit Semaphore(unsigned value = 0);
    ~Semaphore();

    Semaphore(const Semaphore &) = delete;
    Semaphore &operator=(const Semaphore &) = delete;

    void post();
    void wait();
    bool try_wait();

private:
#if defined(__APPLE__)
    semaphore_t sem_;
#elif defined(_WIN32)
    HANDLE sem_;
#else
    sem_t sem_;
#endif
};

#if defined(__APPLE__)
inline Semaphore::Semaphore(unsigned value)
{
    if (semaphore_create(mach_task_self(), &sem_, SYNC_POLICY_FIFO, static_cast<int>(value)) != KERN_SUCCESS)
        throw std::runtime_error("Semaphore::Semaphore");
}

inline Semaphore::~Semaphore()
{
    semaphore_destroy(mach_task_self(), sem_);
}

inline void Semaphore::post()
{
    if (semaphore_signal(sem_) != KERN_SUCCESS)
        throw std::runtime_error("Semaphore::post");
}

inline void Semaphore::wait()
{
    for (;;) {
        switch (semaphore_wait(sem_)) {
        case KERN_SUCCESS:
            return;
        case KERN_ABORTED:
            break;
        default:
            throw std::runtime_error("Semaphore::wait");
        }
    }
}

inline bool Semaphore::try_wait()
{
    for (;;) {
        const mach_timespec_t timeout = {0, 0};
        switch (semaphore_timedwait(sem_, timeout)) {
        case KERN_SUCCESS:
            return true;
        case KERN_OPERATION_TIMED_OUT:
            return false;
        case KERN_ABORTED:
            break;
        default:
            throw std::runtime_error("Semaphore::try_wait");
        }
    }
}
#elif defined(_WIN32)
inline Semaphore::Semaphore(unsigned value)
    : sem_(CreateSemaphore(nullptr, static_cast<LONG>(value), LONG_MAX, nullptr))
{
    if (sem_ == nullptr)
        throw std::runtime_error("Semaphore::Semaphore");
}

inline Semaphore::~Semaphore()
{
    CloseHandle(sem_);
}

inline void Semaphore::post()
{
    if (!ReleaseSemaphore(sem_, 1, nullptr))
        throw std::runtime_error("Semaphore::post");
}

inline void Semaphore::wait()
{
    if (WaitForSingleObject(sem_, INFINITE) != WAIT_OBJECT_0)
        throw std::runtime_error("Semaphore::wait");
}

inline bool Semaphore::try_wait()
{
    switch (WaitForSingleObject(sem_, 0)) {
    case WAIT_OBJECT_0:
        return true;
    case WAIT_TIMEOUT:
        return false;
    default:
        throw std::runtime_error("Semaphore::try_wait");
    }
}
#else
inline Semaphore::Semaphore(unsigned value)
{
    if (sem_init(&sem_, 0, value) != 0)
        throw std::runtime_error("Semaphore::Semaphore");
}

inline Semaphore::~Semaphore()
{
    sem_destroy(&sem_);
}

inline void Semaphore::post()
{
    while (sem_post(&sem_) != 0) {
        if (errno != EINTR)
            throw std::runtime_error("Semaphore::post");
    }
}

inline void Semaphore::wait()
{
    while (sem_wait(&sem_) != 0) {
        if (errno != EINTR)
            throw std::runtime_error("Semaphore::wait");
    }
}

inline bool Semaphore::try_wait()
{
    for (;;) {
        if (sem_trywait(&sem_) == 0)
            return true;
        switch (errno) {
        case EINTR:
            break;
        case EAGAIN:
            return false;
        default:
            throw std::runtime_error("Semaphore::try_wait");
        }
    }
}
#endif
