//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

#pragma once
#include <memory>

#if defined(ADLPLUG_OPL3)
#include <wopl/wopl_file.h>

// wopl_file.h carried these until it moved inside libADLMIDI; upstream
// dropped them there, so they live here now.
struct WOPLFile_Deleter {
    void operator()(WOPLFile *file) const { WOPL_Free(file); }
};
using WOPLFile_Ptr = std::unique_ptr<WOPLFile, WOPLFile_Deleter>;

#elif defined(ADLPLUG_OPN2)
#include <wopn/wopn_file.h>

// Likewise for wopn_file.h inside libOPNMIDI.
struct WOPNFile_Deleter {
    void operator()(WOPNFile *file) const { WOPN_Free(file); }
};
using WOPNFile_Ptr = std::unique_ptr<WOPNFile, WOPNFile_Deleter>;

#endif

struct WOPx {

#if defined(ADLPLUG_OPL3)

#define WOPx_BANK_FORMAT "WOPL"
#define WOPx_BANK_SUFFIX "wopl"
#define WOPx_INST_FORMAT "OPLI"
#define WOPx_INST_SUFFIX "opli"

enum {
    Ins_IsBlank = WOPL_Ins_IsBlank,
};

using Instrument = WOPLInstrument;
using Bank = WOPLBank;
using InstrumentFile = WOPIFile;
using BankFile = WOPLFile;

static constexpr auto &LoadBankFromMem = WOPL_LoadBankFromMem;
static constexpr auto &LoadInstFromMem = WOPL_LoadInstFromMem;
static constexpr auto &CalculateBankFileSize = WOPL_CalculateBankFileSize;
static constexpr auto &CalculateInstFileSize = WOPL_CalculateInstFileSize;
static constexpr auto &SaveBankToMem = WOPL_SaveBankToMem;
static constexpr auto &SaveInstToMem = WOPL_SaveInstToMem;
static constexpr auto &BanksCmp = WOPL_BanksCmp;

using BankFile_Deleter = WOPLFile_Deleter;
using BankFile_Ptr = WOPLFile_Ptr;

#elif defined(ADLPLUG_OPN2)

#define WOPx_BANK_FORMAT "WOPN"
#define WOPx_BANK_SUFFIX "wopn"
#define WOPx_INST_FORMAT "OPNI"
#define WOPx_INST_SUFFIX "opni"

enum {
    Ins_IsBlank = WOPN_Ins_IsBlank,
};

using Instrument = WOPNInstrument;
using Bank = WOPNBank;
using InstrumentFile = OPNIFile;
using BankFile = WOPNFile;

static constexpr auto &LoadBankFromMem = WOPN_LoadBankFromMem;
static constexpr auto &LoadInstFromMem = WOPN_LoadInstFromMem;
static constexpr auto &CalculateBankFileSize = WOPN_CalculateBankFileSize;
static constexpr auto &CalculateInstFileSize = WOPN_CalculateInstFileSize;
static constexpr auto &SaveBankToMem = WOPN_SaveBankToMem;
static constexpr auto &SaveInstToMem = WOPN_SaveInstToMem;
static constexpr auto &BanksCmp = WOPN_BanksCmp;

using BankFile_Deleter = WOPNFile_Deleter;
using BankFile_Ptr = WOPNFile_Ptr;

#endif

};
