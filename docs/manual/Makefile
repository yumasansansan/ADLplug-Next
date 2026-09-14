# SPDX-FileCopyrightText: 2018-2019 Jean Pierre Cimalando
# SPDX-License-Identifier: BSL-1.0
#
# This file comes from ADLplug, which gave it no notice of its own; it is under
# ADLplug's license, the Boost Software License 1.0 (LICENSES/BSL-1.0.txt). The
# SPDX lines name its copyright holder and license in the machine-readable form
# of the REUSE specification.

PREFIX = /usr/local
DOCDIR = $(PREFIX)/share/doc
LANGS = fr en

all:
	$(foreach lang,$(LANGS),$(MAKE) -C $(lang);)

clean:
	$(foreach lang,$(LANGS),$(MAKE) -C $(lang) clean;)

install:
	install -d $(DESTDIR)$(DOCDIR)/ADLplug/manual/
	cp -rf resources $(DESTDIR)$(DOCDIR)/ADLplug/manual/
	$(foreach lang,$(LANGS),install -D $(lang)/manual.html $(DESTDIR)$(DOCDIR)/ADLplug/manual/$(lang)/manual.html;)
