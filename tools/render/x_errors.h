// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

#pragma once

// Makes X errors print a line instead of ending the process.
//
// JUCE installs an X error handler only in applications of its own (a
// JUCEApplication); in a console program such as this one, Xlib's default
// handler exits at the first error. Some requests of JUCE fail where no window
// manager has interned the atoms they use -- a new window gets WM_PROTOCOLS
// whether that atom exists or not -- as on the rootful Xwayland of xwfb-run in
// the CI. JUCE applications, pluginval among them, go on past those errors.
void report_x_errors();
