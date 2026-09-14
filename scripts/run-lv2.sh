#!/bin/sh -e
# SPDX-FileCopyrightText: 2018 Jean Pierre Cimalando
# SPDX-License-Identifier: BSL-1.0
#
# This file comes from ADLplug, which gave it no notice of its own; it is under
# ADLplug's license, the Boost Software License 1.0 (LICENSES/BSL-1.0.txt). The
# SPDX lines name its copyright holder and license in the machine-readable form
# of the REUSE specification.

test -z "$JALV" && JALV=jalv.gtk3

absdir() {
    old=`pwd`; cd "$1"; new=`pwd`; cd "$old"; echo "$new"
}

build_location="`dirname "$0"`/../build"
export LV2_PATH=`absdir "$build_location/lv2"`
exec "$JALV" "https://github.com/jpcima/ADLplug"
