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
