//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

// Included at the end of parameter_block.h.
#include <utility>

template <AudioParameterType Ty, class... Arg>
inline TypedAudioParameter<Ty> *Basic_Parameter_Block::add_automatable_parameter(AudioProcessorEx &p, std::uint32_t tag, Arg &&... args)
{
    TypedAudioParameter<Ty> *par = do_add_parameter<TypedAudioParameter<Ty>>(p, tag, std::forward<Arg>(args)...);
    par->setAutomatable(true);
    return par;
}

template <AudioParameterType Ty, class... Arg>
inline TypedAudioParameter<Ty> *Basic_Parameter_Block::add_parameter(AudioProcessorEx &p, std::uint32_t tag, Arg &&... args)
{
    TypedAudioParameter<Ty> *par = do_add_parameter<TypedAudioParameter<Ty>>(p, tag, std::forward<Arg>(args)...);
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
inline T *Basic_Parameter_Block::do_add_parameter(AudioProcessorEx &p, std::uint32_t tag, const String &id, Arg &&... args)
{
    auto parameter = std::make_unique<T>(ParameterID(id, parameter_version_hint), std::forward<Arg>(args)...);
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
