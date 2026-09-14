// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

#include "x_errors.h"
#include <X11/Xlib.h>
#include <cstdio>

namespace {

int print_x_error(Display *display, XErrorEvent *event)
{
    char text[128];
    XGetErrorText(display, event->error_code, text, static_cast<int>(sizeof text));
    std::fprintf(stderr, "X error, going on: %s (request %u.%u, resource 0x%lx)\n", text,
                 static_cast<unsigned>(event->request_code), static_cast<unsigned>(event->minor_code),
                 event->resourceid);
    return 0;
}

}  // namespace

void report_x_errors()
{
    XSetErrorHandler(print_x_error);
}
