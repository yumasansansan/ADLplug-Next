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
