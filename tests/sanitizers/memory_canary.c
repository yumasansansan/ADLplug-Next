// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// A program that reads a value which was never written, so that a test can show
// MemorySanitizer is really looking. It says nothing when it finds nothing, and
// silence cannot tell "nothing was read uninitialised" from "the check never
// ran": this is the read that has to be reported for the other runs' silence to
// mean anything, as leak_canary.c and race_canary.c are for their sanitizers.
//
// The memory comes from malloc rather than being a local of this function, since
// a local that is read before it is written is something the compiler itself
// warns about, and this project builds with those warnings as errors. It is read
// through a volatile pointer, because an optimised build is free to remove a
// malloc and the free that answers it when nothing else comes of them, and then
// there is no read left to report -- which is what happened before that pointer
// was volatile. Nothing about the read is a matter of timing: every run of this
// reads the same value that was never written, and the sanitizer says where it was
// read and where the memory came from.

#include <stdlib.h>

int main(void)
{
    int *allocated = malloc(sizeof *allocated);
    if (allocated == NULL)
        return EXIT_FAILURE;
    // Never written, on purpose, and read where the build cannot do away with it.
    volatile const int *value = allocated;
    const int read = *value;
    free(allocated);
    // Read rather than only copied: the value decides something.
    if (read == 42)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
