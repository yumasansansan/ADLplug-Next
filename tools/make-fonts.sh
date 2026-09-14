#!/bin/bash -e
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
#
#   tools/make-fonts.sh
#
# Makes the fonts of resources/ui/fonts from the Liberation fonts, version
# 2.00.1 (https://github.com/liberationfonts/liberation-fonts), whose .ttf files
# have to be in the current directory. pyftsubset (fontTools) keeps only the
# characters the editor uses, tools/rename-font.py renames the families to
# ADLplug-Next Mono, Sans and Serif, and zopfli compresses the result. The
# renaming is required: the SIL Open Font License reserves the name Liberation
# for the original fonts, and a subset is a modified version.

tools=$(dirname "$0")
ranges="U+0000-000FF,U+2190-21FF,U+2200-22FF,U+0370-03FF"

convert_font() {
    pyftsubset --unicodes="$ranges" --output-file="subset-$1" "$1"
    python3 "$tools/rename-font.py" "subset-$1" "ADLplug-Next $3"
    #gzip -9 -c "subset-$1" > "$2"
    zopfli -c "subset-$1" > "$2"
    rm -f "subset-$1"
}

for style1 in Mono Sans Serif; do
    for style2 in Regular Bold Italic BoldItalic; do
        convert_font Liberation"$style1"-"$style2".ttf "$style1"-"$style2".ttf.gz "$style1"
    done
done
