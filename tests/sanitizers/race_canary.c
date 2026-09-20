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

#include <pthread.h>
#include <stdlib.h>

static volatile int shared;

static void *write_one(void *unused)
{
    (void)unused;
    shared = 1;
    return NULL;
}

int main(void)
{
    pthread_t other;
    if (pthread_create(&other, NULL, write_one, NULL) != 0)
        return EXIT_FAILURE;
    shared = 2;
    if (pthread_join(other, NULL) != 0)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
