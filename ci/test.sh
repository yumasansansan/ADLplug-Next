#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
#   ci/test.sh <preset>
#
# Runs the tests of a preset that ci/build.sh built. On Linux the editor opens
# on Xwayland under a headless Weston, with a window manager, as it would in a
# Wayland session. On macOS the Audio Unit also goes through auval. The render
# hashes are printed at the end, with a warning when
# tests/render/references.txt has none for this system.
set -euo pipefail

preset=$1
build=build/$preset
status=0

case "$(uname -s)" in
  Linux) xwfb-run -- bash ci/with-window-manager.sh ctest --preset "$preset" || status=$? ;;
  *) ctest --preset "$preset" || status=$? ;;
esac

if [ "$(uname -s)" = Darwin ]; then
  case $preset in
    adl-*) subtype=ADLM ;;
    opn-*) subtype=OPNM ;;
    *) echo "error: no plugin code for preset '$preset'" >&2; exit 2 ;;
  esac
  component=$(find "$build/ADLplug_artefacts" -maxdepth 3 -name '*.component' -print -quit)
  components=$HOME/Library/Audio/Plug-Ins/Components
  mkdir -p "$components"
  rm -rf "${components:?}/$(basename "$component")"
  cp -R "$component" "$components/"
  # The registrar remembers the components it has seen; restarting it makes it
  # look again.
  killall -9 AudioComponentRegistrar 2> /dev/null || true
  echo "== auval"
  auval -v aumu "$subtype" DyTc || status=1
  if command -v pluginval > /dev/null; then
    echo "== pluginval, Audio Unit"
    pluginval --strictness-level 5 --timeout-ms 300000 \
      --validate "$components/$(basename "$component")" || status=1
  fi
fi

render=$build/tests/render
if [ -f "$render/render.hashes" ]; then
  # Windows builds write these with CRLF line ends.
  read -r key plugin < <(tr -d '\r' < "$render/key")
  read -r output state < <(tr -d '\r' < "$render/render.hashes")
  echo "== render hashes: $key $plugin $output $state"
  if [ ! -s "$render/reference" ]; then
    echo "::warning title=No render reference::tests/render/references.txt has no line for $key $plugin. This run gave: $key $plugin $output $state"
  fi
fi

exit "$status"
