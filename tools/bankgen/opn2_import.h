// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// Reads the example banks of OPN2 Bank Editor that are not WOPN files: the GYB
// banks of MIDI2VGM (versions 1 and 2) and GEMS banks. The reading follows the
// editor's own loaders (src/FileFormats/format_m2v_gyb.cpp and
// format_gems_pat.cpp), so that a bank comes out as the editor opens it, and it
// notes what the source holds that a WOPN bank cannot. The instruments have no
// sounding durations yet; the caller measures them.

#pragma once
#include "adl/wopx_file.h"
#include <cstdint>
#include <span>
#include <string>

struct Imported_Bank {
    WOPNFile_Ptr file;
    // Sentences on what the WOPN bank leaves out, one per line.
    std::string notes;
};

// On failure, these return false and say why in `error`.
bool import_gyb(std::span<const std::uint8_t> data, Imported_Bank &bank, std::string &error);
bool import_gems(std::span<const std::uint8_t> data, Imported_Bank &bank, std::string &error);
