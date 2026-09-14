//     Part of ADLplug, distributed under the GNU GPL v3 or later.
//               (See accompanying file LICENSE.)

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
