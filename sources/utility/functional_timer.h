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
#include <memory>
#include <utility>

// Timers made from a callable: create(fn) calls fn(), create1(fn) calls
// fn(timer).
class Functional_Timer : public Timer {
public:
    template <class T> static std::unique_ptr<Timer> create(T fn);
    template <class T> static std::unique_ptr<Timer> create1(T fn);
};

template <class T>
class Functional_Timer_T final : public Functional_Timer {
public:
    explicit Functional_Timer_T(T fn) : fn_(std::move(fn)) {}
    void timerCallback() override { fn_(); }
private:
    T fn_;
};

template <class T>
std::unique_ptr<Timer> Functional_Timer::create(T fn)
{
    return std::make_unique<Functional_Timer_T<T>>(std::move(fn));
}

template <class T>
class Functional_Timer1_T final : public Functional_Timer {
public:
    explicit Functional_Timer1_T(T fn) : fn_(std::move(fn)) {}
    void timerCallback() override { fn_(this); }
private:
    T fn_;
};

template <class T>
std::unique_ptr<Timer> Functional_Timer::create1(T fn)
{
    return std::make_unique<Functional_Timer1_T<T>>(std::move(fn));
}
