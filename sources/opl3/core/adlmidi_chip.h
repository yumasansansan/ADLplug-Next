// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// What libADLMIDI calls to make ADLplug-Next's OPL3 core, emulator 14. The
// library declares it in the same words
// (patches/libADLMIDI/0010-emulator-14-is-adlplug-next-s-own-opl3-core.patch).

#pragma once

class OPLChipBase;

namespace adlplug::opl3 {

OPLChipBase *create_adlmidi_chip();

}  // namespace adlplug::opl3
