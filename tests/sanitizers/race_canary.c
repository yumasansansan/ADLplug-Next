// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// A program that races on purpose, so that a test can show ThreadSanitizer is
// really looking. It says nothing when it finds nothing, and silence cannot
// tell "no races" from "the check never ran": this is the race that has to be
// reported for the other tests' silence to mean anything.
//
// Two threads write the same object with nothing to order the one against the
// other, which is the whole of what the program does. The object is volatile,
// so that an optimised build still has the writes for the sanitizer to see.
//
// The two threads meet before they write. Without that, a program this short can
// have the one thread write and finish before the other starts, and then the
// sanitizer has two writes that never overlapped in time: the race is there in
// the program, and a run of it need not be one where the sanitizer names it. A
// test of the sanitizer has to fail only when the sanitizer is not looking, so the
// writes are made to overlap rather than left to chance -- both threads wait until
// both have arrived, and only then write, over and over. The waiting is itself
// ordering, and it orders what came before it; what the threads do afterwards is
// as unordered as it ever was.
//
// The meeting is made of a mutex and a condition variable rather than a barrier,
// since pthread_barrier_* is an option of POSIX that this project's strict C23 does
// not have the declarations of, while mutexes and condition variables are always
// declared.

#include <pthread.h>
#include <stdlib.h>

static volatile int shared;

// How many times each thread writes. One write from each would be enough for the
// sanitizer to compare; a thousand is so that a report does not depend on which
// of the two gets there first.
enum { writes = 1000 };

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t arrived = PTHREAD_COND_INITIALIZER;
static unsigned waiting;

// Returns when both threads have called it, so that neither writes alone.
static int meet(void)
{
    if (pthread_mutex_lock(&lock) != 0)
        return 0;
    ++waiting;
    if (waiting == 2 && pthread_cond_broadcast(&arrived) != 0) {
        pthread_mutex_unlock(&lock);
        return 0;
    }
    while (waiting < 2) {
        if (pthread_cond_wait(&arrived, &lock) != 0) {
            pthread_mutex_unlock(&lock);
            return 0;
        }
    }
    return pthread_mutex_unlock(&lock) == 0;
}

static void *write_one(void *unused)
{
    (void)unused;
    if (!meet())
        return NULL;
    for (int i = 0; i < writes; ++i)
        shared = 1;
    return NULL;
}

int main(void)
{
    pthread_t other;
    if (pthread_create(&other, NULL, write_one, NULL) != 0)
        return EXIT_FAILURE;
    if (!meet())
        return EXIT_FAILURE;
    for (int i = 0; i < writes; ++i)
        shared = 2;
    if (pthread_join(other, NULL) != 0)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
