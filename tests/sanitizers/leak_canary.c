// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// A program that leaks on purpose, so that a test can show LeakSanitizer is
// really looking (plan D47). LeakSanitizer says nothing when it finds nothing,
// and silence cannot tell "no leaks" from "the check never ran": this is the
// leak that has to be reported for the other tests' silence to mean anything.
//
// The block is dropped in a function of its own, which the compiler may not
// inline, so that no register or stack slot of main still holds its address
// when the check runs at exit.

#include <stdlib.h>
#include <string.h>

static void *volatile lost;

[[gnu::noinline]] static void lose(void)
{
    lost = malloc(64);
    memset(lost, 0x55, 64);
    lost = NULL;
}

int main(void)
{
    lose();
    return 0;
}
