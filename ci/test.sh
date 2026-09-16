#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
#   ci/test.sh <preset> [--no-gui] [--no-hosts]
#
# Runs the tests of a preset that ci/build.sh built. On Linux the editor opens
# on Xwayland under a headless Weston, with a window manager, as it would in a
# Wayland session; --no-gui leaves out the tests that open windows instead, for
# systems that have none of those, as the AlmaLinux container of CI. On macOS
# the Audio Unit also goes through auval. --no-hosts leaves out the programs
# that load the plugins from outside, pluginval, lv2lint and auval: a build made
# with the sanitizers cannot be loaded into them, since they are built without.
# The render hashes are printed at the end, with a warning when
# tests/render/references.txt has none for this system.
set -euo pipefail

preset=$1
shift
gui=true
hosts=true
for argument in "$@"; do
  case $argument in
    --no-gui) gui=false ;;
    --no-hosts) hosts=false ;;
    *) echo "error: unknown argument '$argument'" >&2; exit 2 ;;
  esac
done
build=build/$preset
status=0

exclude=()
if [ "$hosts" = false ]; then
  exclude=(--exclude-regex 'pluginval|lv2lint')
fi

if [ "$gui" = false ]; then
  ctest --preset "$preset" --label-exclude gui ${exclude[@]+"${exclude[@]}"} || status=$?
else
  case "$(uname -s)" in
    Linux) xwfb-run -- bash ci/with-window-manager.sh ctest --preset "$preset" ${exclude[@]+"${exclude[@]}"} || status=$? ;;
    *) ctest --preset "$preset" ${exclude[@]+"${exclude[@]}"} || status=$? ;;
  esac
fi

if [ "$(uname -s)" = Darwin ] && [ "$hosts" = true ]; then
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
