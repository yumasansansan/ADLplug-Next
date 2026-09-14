//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// SPDX-FileCopyrightText: 2018 Jean Pierre Cimalando
// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: BSL-1.0 AND GPL-3.0-or-later
//
// This file comes from ADLplug and was modified for ADLplug-Next. The notice at
// the top is ADLplug's; the LICENSE it names was ADLplug's copy of the Boost
// Software License, now LICENSES/BSL-1.0.txt. The SPDX lines name the copyright
// holders and licenses in the machine-readable form of the REUSE specification:
// ADLplug's code is under the Boost Software License 1.0, and ADLplug-Next's
// changes are under the GNU General Public License, version 3 or any later
// version (LICENSES/GPL-3.0-or-later.txt).

#pragma once
#include <unordered_map>

enum class Midi_Spec {
    GM, GS, SC88, MT32, XG,
};

const char *midi_spec_name(Midi_Spec spec);

struct Midi_Program_Ex {
    Midi_Spec spec {};
    const char *name = nullptr;
};

// Names of the General MIDI programs and percussion keys, and of the melodic
// programs which the GS and XG extensions put in other banks.
class Midi_Db {
public:
    const char *inst(unsigned id7) const noexcept
        { return midi_inst_[id7 & 127]; }
    const Midi_Program_Ex &perc(unsigned id7) const noexcept
        { return midi_perc_[id7 & 127]; }
    // Null if the bank has no name of its own for the program. A program
    // number above 127 denotes a percussion key, which never has one.
    const Midi_Program_Ex *find_ex(unsigned msb, unsigned lsb, unsigned pgm) const;

private:
    friend const Midi_Db &midi_db();
    Midi_Db();

    const char *midi_inst_[128] {};
    Midi_Program_Ex midi_perc_[128] {};
    std::unordered_map<unsigned, Midi_Program_Ex> midi_ex_;

    void init_midi_inst();
    void init_midi_perc();
    void init_midi_ex();
};

// The database, built on first use.
const Midi_Db &midi_db();
