//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.
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
extern const struct Res_Data adlplug_res_emu_dosbox;
extern const struct Res_Data adlplug_res_emu_nuked;
extern const struct Res_Data adlplug_res_emu_nuked2;
extern const struct Res_Data adlplug_res_emu_opal;
extern const struct Res_Data adlplug_res_emu_java;
extern const struct Res_Data adlplug_res_emoji_u1f4a1;
#elif defined(ADLPLUG_OPN2)
extern const struct Res_Data adlplug_res_opn2_banks_pak;
extern const struct Res_Data adlplug_res_emu_mame;
extern const struct Res_Data adlplug_res_emu_nuked;
extern const struct Res_Data adlplug_res_emu_gens;
extern const struct Res_Data adlplug_res_emu_neko;
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
    inline constexpr const Data &emu_dosbox = adlplug_res_emu_dosbox;
    inline constexpr const Data &emu_nuked = adlplug_res_emu_nuked;
    inline constexpr const Data &emu_nuked2 = adlplug_res_emu_nuked2;
    inline constexpr const Data &emu_opal = adlplug_res_emu_opal;
    inline constexpr const Data &emu_java = adlplug_res_emu_java;
    inline constexpr const Data &emoji_u1f4a1 = adlplug_res_emoji_u1f4a1;
#elif defined(ADLPLUG_OPN2)
    inline constexpr const Data &banks_pak = adlplug_res_opn2_banks_pak;
    inline constexpr const Data &emu_mame = adlplug_res_emu_mame;
    inline constexpr const Data &emu_nuked = adlplug_res_emu_nuked;
    inline constexpr const Data &emu_gens = adlplug_res_emu_gens;
    inline constexpr const Data &emu_neko = adlplug_res_emu_neko;
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
