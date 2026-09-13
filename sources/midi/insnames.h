//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

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
