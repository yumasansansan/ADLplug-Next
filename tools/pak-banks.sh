#!/bin/bash -e
# SPDX-FileCopyrightText: 2018 Jean Pierre Cimalando
# SPDX-License-Identifier: BSL-1.0
#
# This file comes from ADLplug, which gave it no notice of its own; it is under
# ADLplug's license, the Boost Software License 1.0 (LICENSES/BSL-1.0.txt). The
# SPDX lines name its copyright holder and license in the machine-readable form
# of the REUSE specification.

banks=adldata/*

# Compress
cat $banks > _all.wopl
zopfli _all.wopl

# Write dictionary
offset=0
for bank in $banks; do
    filename=`basename "$bank"`
    filename=`echo "$filename" | sed -r 's/^[0-9]{3} //'`
    size=`stat -c '%s' "$bank"`
    printf '%.8x' "$size" | xxd -r -p
    printf '%.8x' "$offset" | xxd -r -p
    printf '%s' "$filename"
    printf '%.2x' 0 | xxd -r -p
    offset=$((offset+size))
done
printf '%.8x' 0 | xxd -r -p

# Write file data
cat _all.wopl.gz

# Clean up
rm _all.wopl.gz _all.wopl
