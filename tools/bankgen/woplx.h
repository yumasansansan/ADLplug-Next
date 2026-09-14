// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// Reads the WOPLX banks of libADLMIDI (fm_banks_new/*.woplx), the text form in
// which libADLMIDI keeps the banks it builds in. The reading follows
// BankFormats::LoadWoplX() of libADLMIDI's gen_adldata, and keeps everything a
// file holds in a WOPL bank: its notes, flags and volume model, and the names,
// MIDI numbers and instruments of its melodic and percussion banks. A line, key
// or value it does not know is an error, so that nothing is left out unseen.

#pragma once
#include "adl/wopx_file.h"
#include <string>

struct Woplx_Bank {
    WOPLFile_Ptr file;
    // The lines between BANK_INFO: and BANK_INFO_END, as they are.
    std::string info;
};

// Reads a WOPLX bank from its text. On failure, returns false and says why in
// `error`.
bool read_woplx(const std::string &text, Woplx_Bank &bank, std::string &error);
