//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

// Included at the end of parameter_ex.h.

template <class Parameter>
void AudioParameterEx<Parameter>::addValueChangedListenerEx(ValueChangedListener *l)
{
    const ScopedLock sl(listener_lock_);
    listeners_.addIfNotAlreadyThere(l);
}

template <class Parameter>
void AudioParameterEx<Parameter>::removeValueChangedListenerEx(ValueChangedListener *l)
{
    const ScopedLock sl(listener_lock_);
    listeners_.removeFirstMatchingValue(l);
}

template <class Parameter>
void AudioParameterEx<Parameter>::invoke_value_changed_listeners()
{
    const ScopedLock sl(listener_lock_);
    for (int i = listeners_.size(); i-- > 0;)
        listeners_.getUnchecked(i)->parameterValueChangedEx(tag_);
}

template <class Parameter>
std::uint32_t AudioParameterEx<Parameter>::getTagEx() const noexcept
{
    return tag_;
}

template <class Parameter>
void AudioParameterEx<Parameter>::setTagEx(std::uint32_t tag) noexcept
{
    tag_ = tag;
}

template <class Parameter>
void AudioParameterEx<Parameter>::setAutomatable(bool automatable)
{
    automatable_ = automatable;
}

template <class Parameter>
bool AudioParameterEx<Parameter>::isAutomatable() const
{
    return automatable_ ? *automatable_ : Parameter::isAutomatable();
}
