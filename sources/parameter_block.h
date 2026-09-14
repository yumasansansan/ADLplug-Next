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
#include "JuceHeader.h"
#include "utility/processor_ex.h"
#include "utility/parameter_ex.h"
#include "utility/fourcc.h"
#include <cstdint>
#include <memory>
#include <vector>

// Tags tell the processor which part of its state a parameter belongs to.
namespace Parameter_Tag {
    inline constexpr std::uint32_t chip = fourcc("chip");
    inline constexpr std::uint32_t global = fourcc("glob");

    // "ins" in the upper three bytes, the part number in the lowest.
    constexpr std::uint32_t instrument(unsigned part) noexcept
        { return fourcc("ins\0") | (part & 0xff); }
    constexpr bool is_instrument(std::uint32_t tag) noexcept
        { return (tag & 0xffffff00) == fourcc("ins\0"); }
    constexpr unsigned part_of(std::uint32_t tag) noexcept
        { return tag & 0xff; }
}

// The version hint of a parameter (JUCE's ParameterID). JUCE's Audio Unit
// wrapper lists parameters in the order of their hints, so that hosts can keep
// the ones they know in place: a parameter added later needs a higher hint
// than those before it. The parameters of ADLplug 1 have hint 1; those added
// since give theirs to add_parameter_since().
inline constexpr int parameter_version_hint = 1;

struct Basic_Parameter_Block {
    template <AudioParameterType Ty, class... Arg>
    TypedAudioParameter<Ty> *add_automatable_parameter(AudioProcessorEx &p, std::uint32_t tag, Arg &&... args);

    template <AudioParameterType Ty, class... Arg>
    TypedAudioParameter<Ty> *add_parameter(AudioProcessorEx &p, std::uint32_t tag, Arg &&... args);

    // A parameter added after ADLplug 1, with the version hint it came with.
    template <AudioParameterType Ty, class... Arg>
    TypedAudioParameter<Ty> *add_parameter_since(int version_hint, AudioProcessorEx &p, std::uint32_t tag, Arg &&... args);

    template <AudioParameterType Ty, class... Arg>
    TypedAudioParameter<Ty> *add_internal_parameter(AudioProcessorEx &p, std::uint32_t tag, Arg &&... args);

private:
    template <class T, class... Arg>
    T *do_add_parameter(AudioProcessorEx &p, std::uint32_t tag, int version_hint, const String &id, Arg &&... args);

    template <class T, class... Arg>
    T *do_add_internal_parameter(AudioProcessorEx &p, std::uint32_t tag, const String &id, Arg &&... args);

    std::vector<std::unique_ptr<AudioProcessorParameter>> internal_parameters_;
};

#if defined(ADLPLUG_OPL3)
#include "opl3/parameter_block.h"
#elif defined(ADLPLUG_OPN2)
#include "opn2/parameter_block.h"
#endif

#include "parameter_block.tcc"
