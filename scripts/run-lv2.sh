#!/bin/sh -e
# SPDX-FileCopyrightText: 2018 Jean Pierre Cimalando
# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: BSL-1.0 AND GPL-3.0-or-later
#
# This file comes from ADLplug and was modified for ADLplug-Next. ADLplug gave
# it no notice of its own; it was under ADLplug's license, the Boost Software
# License 1.0 (LICENSES/BSL-1.0.txt). The SPDX lines name the copyright holders
# and licenses in the machine-readable form of the REUSE specification:
# ADLplug's part is under the Boost Software License 1.0, and ADLplug-Next's
# changes are under the GNU General Public License, version 3 or any later
# version (LICENSES/GPL-3.0-or-later.txt).

test -z "$JALV" && JALV=jalv.gtk3

absdir() {
    old=`pwd`; cd "$1"; new=`pwd`; cd "$old"; echo "$new"
}

build_location="`dirname "$0"`/../build"
export LV2_PATH=`absdir "$build_location/lv2"`
exec "$JALV" "https://github.com/yumasansansan/ADLplug-Next#ADLplug-Next"
