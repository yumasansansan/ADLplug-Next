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
#include <cstdint>
#include <optional>

namespace AudioParametersEx {
    class ValueChangedListener {
    public:
        virtual ~ValueChangedListener() = default;
        virtual void parameterValueChangedEx(std::uint32_t) {}
    };
}

template <class Parameter>
class AudioParameterEx : public Parameter {
public:
    using Parameter::Parameter;
    using ValueChangedListener = AudioParametersEx::ValueChangedListener;

    void addValueChangedListenerEx(ValueChangedListener *l);
    void removeValueChangedListenerEx(ValueChangedListener *l);

    std::uint32_t getTagEx() const noexcept;
    void setTagEx(std::uint32_t tag) noexcept;

    void setAutomatable(bool automatable);
    bool isAutomatable() const override;

protected:
    void invoke_value_changed_listeners();

private:
    CriticalSection listener_lock_;
    Array<ValueChangedListener *> listeners_;
    std::uint32_t tag_ = 0;
    std::optional<bool> automatable_;
};

//------------------------------------------------------------------------------
class AudioParameterExBool : public AudioParameterEx<AudioParameterBool> {
public:
    using AudioParameterEx::AudioParameterEx;
protected:
    void valueChanged(bool) override
        { invoke_value_changed_listeners(); }
};

class AudioParameterExChoice : public AudioParameterEx<AudioParameterChoice> {
public:
    using AudioParameterEx::AudioParameterEx;
protected:
    void valueChanged(int) override
        { invoke_value_changed_listeners(); }
};

class AudioParameterExFloat : public AudioParameterEx<AudioParameterFloat> {
public:
    using AudioParameterEx::AudioParameterEx;
protected:
    void valueChanged(float) override
        { invoke_value_changed_listeners(); }
};

class AudioParameterExInt : public AudioParameterEx<AudioParameterInt> {
public:
    using AudioParameterEx::AudioParameterEx;
protected:
    void valueChanged(int) override
        { invoke_value_changed_listeners(); }
};

//------------------------------------------------------------------------------
enum class AudioParameterType {
    Bool, Choice, Float, Int,
};

//------------------------------------------------------------------------------
template <AudioParameterType Ty>
struct TypedAudioParameterTraits;

template <>
struct TypedAudioParameterTraits<AudioParameterType::Bool> {
    using type = AudioParameterExBool;
};

template <>
struct TypedAudioParameterTraits<AudioParameterType::Choice> {
    using type = AudioParameterExChoice;
};

template <>
struct TypedAudioParameterTraits<AudioParameterType::Float> {
    using type = AudioParameterExFloat;
};

template <>
struct TypedAudioParameterTraits<AudioParameterType::Int> {
    using type = AudioParameterExInt;
};

template <AudioParameterType Ty>
using TypedAudioParameter = TypedAudioParameterTraits<Ty>::type;

#include "parameter_ex.tcc"
