// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// The one thing the audio thread does that the realtime sanitizer forbids, and
// where this project says so.
//
// processBlock and what it calls are marked [[clang::nonblocking]]
// (plugin_processor.h): nothing under them may take a lock, allocate, free, read a
// file or throw, because a moment's wait there is a gap in the sound. The
// sanitizer holds them to it, and over every input this project keeps it found one
// thing to say, in two places: pthread_mutex_unlock.
//
// It is right to say it. Releasing a mutex wakes whoever waits on it, and waking a
// thread is a system call; the sanitizer cannot see that this mutex was taken with
// try_to_lock and so waited for nobody, nor that nothing waits on it in the
// ordinary case, where releasing it is one atomic write.
//
// It is also the plugin's decision, made before this sanitizer was here. The audio
// thread and the editor share the player: the editor's side takes the player's lock
// and holds it while it measures an instrument or rebuilds the chips, and the audio
// thread *tries* for it and plays silence for one block if it cannot have it
// (plugin_processor.cc). It never waits. The other ways round would be worse: to
// wait is to make the gap certain, and to hand the player over without a lock at
// all is to read it while it is being rebuilt.
//
// So the release is marked rather than the sanitizer quieted: the scope below turns
// the check off for as long as the release takes and no longer, which leaves
// everything else under processBlock watched -- the synthesis above all. Nothing
// else in this project may use it without a reason of its own written beside it.

#pragma once
#include <mutex>

#if defined(__has_feature)
#  if __has_feature(realtime_sanitizer)
#    include <sanitizer/rtsan_interface.h>
#    define ADLPLUG_REALTIME_SANITIZER 1
#  endif
#endif

// A scope the realtime sanitizer does not watch. In a build without that
// sanitizer both of these are empty, and the destructor is written out rather than
// defaulted so that an object of this type is never an unused variable.
class Realtime_Check_Off {
public:
    Realtime_Check_Off() noexcept
    {
#if defined(ADLPLUG_REALTIME_SANITIZER)
        __rtsan_disable();
#endif
    }
    ~Realtime_Check_Off()
    {
#if defined(ADLPLUG_REALTIME_SANITIZER)
        __rtsan_enable();
#endif
    }
    Realtime_Check_Off(const Realtime_Check_Off &) = delete;
    Realtime_Check_Off &operator=(const Realtime_Check_Off &) = delete;
};

// Releases a lock the audio thread holds, if it holds it. The check is off for
// that and for nothing else.
inline void release_on_audio_thread(std::unique_lock<std::mutex> &lock)
{
    if (!lock.owns_lock())
        return;
    const Realtime_Check_Off unchecked;
    lock.unlock();
}
