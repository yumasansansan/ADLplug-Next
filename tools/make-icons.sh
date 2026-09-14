#!/bin/bash
# SPDX-FileCopyrightText: 2018 Jean Pierre Cimalando
# SPDX-License-Identifier: BSL-1.0
#
# This file comes from ADLplug, which gave it no notice of its own; it is under
# ADLplug's license, the Boost Software License 1.0 (LICENSES/BSL-1.0.txt). The
# SPDX lines name its copyright holder and license in the machine-readable form
# of the REUSE specification.

set -e

for icon in ADLplug OPNplug; do
  for size in 32 96; do
    convert -resize "$size"x"$size" "$icon".png "$icon"-"$size".png
    optipng "$icon"-"$size".png
  done
done
