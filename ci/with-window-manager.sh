#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
#   ci/with-window-manager.sh <command> [<argument>...]
#
# Runs a command on $DISPLAY with a window manager, Openbox, the way a desktop
# session has one. JUCE sets WM_PROTOCOLS on every window it creates without
# checking that the atom exists; on an X server where no window manager has
# made it, the request fails with BadAtom, and a host with no X error handler of
# its own ends there. lv2lint does, opening the LV2 editor. The Xwayland of
# xwfb-run is rootful and has no window manager.
set -euo pipefail

ready=$(mktemp -d)
# Openbox runs the --startup command once it has taken over the screen.
openbox --sm-disable --startup "touch $ready/started" &
wm=$!
for _ in $(seq 100); do
  if [ -e "$ready/started" ] || ! kill -0 "$wm" 2> /dev/null; then
    break
  fi
  sleep 0.1
done
if [ ! -e "$ready/started" ]; then
  echo "error: Openbox did not start" >&2
  kill "$wm" 2> /dev/null || true
  exit 1
fi
rm -rf "$ready"

status=0
"$@" || status=$?
kill "$wm" 2> /dev/null || true
wait "$wm" 2> /dev/null || true
exit "$status"
