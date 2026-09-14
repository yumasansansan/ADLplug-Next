#!/usr/bin/env bash
# Part of ADLplug, distributed under the GNU GPL v3 or later.
#               (See accompanying file LICENSE.)
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
  auval -v aumu "$subtype" JPCm || status=1
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
