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
//
// Binary resources embedded by resources.c. This header is valid C and C++;
// C++ code refers to the resources as Res::<name>.

#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Res_Data {
    const unsigned char *data;
    size_t size;
};

#if defined(ADLPLUG_OPL3)
extern const struct Res_Data adlplug_res_opl3_banks_pak;
extern const struct Res_Data adlplug_res_emu_nuked;
extern const struct Res_Data adlplug_res_emu_nuked2;
extern const struct Res_Data adlplug_res_emu_esfmu;
extern const struct Res_Data adlplug_res_emoji_u1f4a1;
#elif defined(ADLPLUG_OPN2)
extern const struct Res_Data adlplug_res_opn2_banks_pak;
extern const struct Res_Data adlplug_res_emu_nuked;
#endif

extern const struct Res_Data adlplug_res_emoji_u1f4be;
extern const struct Res_Data adlplug_res_emoji_u1f4c2;
extern const struct Res_Data adlplug_res_emoji_u1f4dd;
extern const struct Res_Data adlplug_res_emoji_u2328;
extern const struct Res_Data adlplug_res_emoji_u2795;
extern const struct Res_Data adlplug_res_knob_skin;
extern const struct Res_Data adlplug_res_slider_skin;
extern const struct Res_Data adlplug_res_Mono_BoldItalic;
extern const struct Res_Data adlplug_res_Mono_Bold;
extern const struct Res_Data adlplug_res_Mono_Italic;
extern const struct Res_Data adlplug_res_Mono_Regular;
extern const struct Res_Data adlplug_res_Sans_BoldItalic;
extern const struct Res_Data adlplug_res_Sans_Bold;
extern const struct Res_Data adlplug_res_Sans_Italic;
extern const struct Res_Data adlplug_res_Sans_Regular;
extern const struct Res_Data adlplug_res_Serif_BoldItalic;
extern const struct Res_Data adlplug_res_Serif_Bold;
extern const struct Res_Data adlplug_res_Serif_Italic;
extern const struct Res_Data adlplug_res_Serif_Regular;

#ifdef __cplusplus
}  // extern "C"

namespace Res {
    using Data = Res_Data;

#if defined(ADLPLUG_OPL3)
    inline constexpr const Data &banks_pak = adlplug_res_opl3_banks_pak;
    inline constexpr const Data &emu_nuked = adlplug_res_emu_nuked;
    inline constexpr const Data &emu_nuked2 = adlplug_res_emu_nuked2;
    inline constexpr const Data &emu_esfmu = adlplug_res_emu_esfmu;
    inline constexpr const Data &emoji_u1f4a1 = adlplug_res_emoji_u1f4a1;
#elif defined(ADLPLUG_OPN2)
    inline constexpr const Data &banks_pak = adlplug_res_opn2_banks_pak;
    inline constexpr const Data &emu_nuked = adlplug_res_emu_nuked;
#endif

    inline constexpr const Data &emoji_u1f4be = adlplug_res_emoji_u1f4be;
    inline constexpr const Data &emoji_u1f4c2 = adlplug_res_emoji_u1f4c2;
    inline constexpr const Data &emoji_u1f4dd = adlplug_res_emoji_u1f4dd;
    inline constexpr const Data &emoji_u2328 = adlplug_res_emoji_u2328;
    inline constexpr const Data &emoji_u2795 = adlplug_res_emoji_u2795;
    inline constexpr const Data &knob_skin = adlplug_res_knob_skin;
    inline constexpr const Data &slider_skin = adlplug_res_slider_skin;
    inline constexpr const Data &Mono_BoldItalic = adlplug_res_Mono_BoldItalic;
    inline constexpr const Data &Mono_Bold = adlplug_res_Mono_Bold;
    inline constexpr const Data &Mono_Italic = adlplug_res_Mono_Italic;
    inline constexpr const Data &Mono_Regular = adlplug_res_Mono_Regular;
    inline constexpr const Data &Sans_BoldItalic = adlplug_res_Sans_BoldItalic;
    inline constexpr const Data &Sans_Bold = adlplug_res_Sans_Bold;
    inline constexpr const Data &Sans_Italic = adlplug_res_Sans_Italic;
    inline constexpr const Data &Sans_Regular = adlplug_res_Sans_Regular;
    inline constexpr const Data &Serif_BoldItalic = adlplug_res_Serif_BoldItalic;
    inline constexpr const Data &Serif_Bold = adlplug_res_Serif_Bold;
    inline constexpr const Data &Serif_Italic = adlplug_res_Serif_Italic;
    inline constexpr const Data &Serif_Regular = adlplug_res_Serif_Regular;
}
#endif
