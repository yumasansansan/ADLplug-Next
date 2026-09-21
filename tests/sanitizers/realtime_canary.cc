// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// A program that waits where it must not, so that a test can show
// RealtimeSanitizer is really looking. It says nothing when it finds nothing, and
// silence cannot tell that from a check which never ran: this is the wait that has
// to be reported for the other runs' silence to mean anything, as leak_canary.c,
// race_canary.c and memory_canary.c are for their sanitizers.
//
// A function marked [[clang::nonblocking]] takes a lock, which is the plainest
// thing of its kind: the sanitizer intercepts pthread_mutex_lock and says where it
// was called from. Nothing about it is a matter of timing, and nothing an optimiser
// does can remove it -- a mutex another translation unit could hold has to be
// locked for real.
//
// This one is C++ where the others are C, because the attribute is what the
// sanitizer watches for and Clang acts on it in C++ alone: the same program in C23,
// with the attribute in the same place, is not watched and reports nothing
// (measured with Clang 23).

#include <mutex>

namespace {

std::mutex somewhere_else;

void on_the_audio_thread() [[clang::nonblocking]]
{
    const std::scoped_lock lock(somewhere_else);
}

}  // namespace

int main()
{
    on_the_audio_thread();
    return 0;
}
