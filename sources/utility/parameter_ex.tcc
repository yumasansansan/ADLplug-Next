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

// Included at the end of parameter_ex.h.

template <class Parameter>
void AudioParameterEx<Parameter>::addValueChangedListenerEx(ValueChangedListener *l)
{
    listeners_.addIfNotAlreadyThere(l);
}

// Called wherever a value changes, which includes the audio thread: a program
// change has it set every parameter of an instrument. There is no lock, and the
// header says what makes that sound -- the list is written while the processor is
// made and read for ever after. There was one, and the realtime sanitizer found
// the audio thread waiting on it.
template <class Parameter>
void AudioParameterEx<Parameter>::invoke_value_changed_listeners()
{
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
