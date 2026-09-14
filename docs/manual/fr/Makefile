# SPDX-FileCopyrightText: 2018 Jean Pierre Cimalando
# SPDX-License-Identifier: BSL-1.0
#
# This file comes from ADLplug, which gave it no notice of its own; it is under
# ADLplug's license, the Boost Software License 1.0 (LICENSES/BSL-1.0.txt). The
# SPDX lines name its copyright holder and license in the machine-readable form
# of the REUSE specification.

ADOC = asciidoctor
ADOCFLAGS = -a docdate="$(shell date "+%d/%m/%Y")" -a doctime="$(shell date "+%R %Z")"

all:
	$(ADOC) $(ADOCFLAGS) manual.txt

clean:
	rm -f manual.html

auto:
	while true; do $(ADOC) $(ADOCFLAGS) manual.txt; \
	    inotifywait -q -e close_write manual.txt; done
