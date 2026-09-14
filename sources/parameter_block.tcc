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

// Included at the end of parameter_block.h.
#include <utility>

template <AudioParameterType Ty, class... Arg>
inline TypedAudioParameter<Ty> *Basic_Parameter_Block::add_automatable_parameter(AudioProcessorEx &p, std::uint32_t tag, Arg &&... args)
{
    TypedAudioParameter<Ty> *par = do_add_parameter<TypedAudioParameter<Ty>>(p, tag, parameter_version_hint, std::forward<Arg>(args)...);
    par->setAutomatable(true);
    return par;
}

template <AudioParameterType Ty, class... Arg>
inline TypedAudioParameter<Ty> *Basic_Parameter_Block::add_parameter(AudioProcessorEx &p, std::uint32_t tag, Arg &&... args)
{
    TypedAudioParameter<Ty> *par = do_add_parameter<TypedAudioParameter<Ty>>(p, tag, parameter_version_hint, std::forward<Arg>(args)...);
    par->setAutomatable(false);
    return par;
}

template <AudioParameterType Ty, class... Arg>
inline TypedAudioParameter<Ty> *Basic_Parameter_Block::add_parameter_since(int version_hint, AudioProcessorEx &p, std::uint32_t tag, Arg &&... args)
{
    TypedAudioParameter<Ty> *par = do_add_parameter<TypedAudioParameter<Ty>>(p, tag, version_hint, std::forward<Arg>(args)...);
    par->setAutomatable(false);
    return par;
}

template <AudioParameterType Ty, class... Arg>
inline TypedAudioParameter<Ty> *Basic_Parameter_Block::add_internal_parameter(AudioProcessorEx &p, std::uint32_t tag, Arg &&... args)
{
    TypedAudioParameter<Ty> *par = do_add_internal_parameter<TypedAudioParameter<Ty>>(p, tag, std::forward<Arg>(args)...);
    par->setAutomatable(false);
    return par;
}

// The processor owns external parameters; internal ones stay with the block.
template <class T, class... Arg>
inline T *Basic_Parameter_Block::do_add_parameter(AudioProcessorEx &p, std::uint32_t tag, int version_hint, const String &id, Arg &&... args)
{
    auto parameter = std::make_unique<T>(ParameterID(id, version_hint), std::forward<Arg>(args)...);
    T *raw = parameter.get();
    raw->setTagEx(tag);
    raw->addValueChangedListenerEx(&p);
    p.addParameter(parameter.release());
    return raw;
}

template <class T, class... Arg>
inline T *Basic_Parameter_Block::do_add_internal_parameter(AudioProcessorEx &p, std::uint32_t tag, const String &id, Arg &&... args)
{
    auto parameter = std::make_unique<T>(ParameterID(id, parameter_version_hint), std::forward<Arg>(args)...);
    T *raw = parameter.get();
    raw->setTagEx(tag);
    raw->addValueChangedListenerEx(&p);
    internal_parameters_.push_back(std::move(parameter));
    return raw;
}
