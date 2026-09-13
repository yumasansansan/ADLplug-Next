//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

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

struct Basic_Parameter_Block {
    template <AudioParameterType Ty, class... Arg>
    TypedAudioParameter<Ty> *add_automatable_parameter(AudioProcessorEx &p, std::uint32_t tag, Arg &&... args);

    template <AudioParameterType Ty, class... Arg>
    TypedAudioParameter<Ty> *add_parameter(AudioProcessorEx &p, std::uint32_t tag, Arg &&... args);

    template <AudioParameterType Ty, class... Arg>
    TypedAudioParameter<Ty> *add_internal_parameter(AudioProcessorEx &p, std::uint32_t tag, Arg &&... args);

private:
    template <class T, class... Arg>
    T *do_add_parameter(AudioProcessorEx &p, std::uint32_t tag, Arg &&... args);

    template <class T, class... Arg>
    T *do_add_internal_parameter(AudioProcessorEx &p, std::uint32_t tag, Arg &&... args);

    std::vector<std::unique_ptr<AudioProcessorParameter>> internal_parameters_;
};

#if defined(ADLPLUG_OPL3)
#include "opl3/parameter_block.h"
#elif defined(ADLPLUG_OPN2)
#include "opn2/parameter_block.h"
#endif

#include "parameter_block.tcc"
