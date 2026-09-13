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

class Info_Display : protected Timer {
public:
    ~Info_Display() override = default;

    void set_default_info(const String &text);
    void display_info(const String &text);
    void expire_info_in(int timeout_ms = 3000);

protected:
    virtual void display_info_now(const String &text) = 0;

private:
    void timerCallback() override;
    String default_text_;
};

//==============================================================================
inline void Info_Display::set_default_info(const String &text)
{
    default_text_ = text;
}

inline void Info_Display::display_info(const String &text)
{
    stopTimer();
    display_info_now(text);
}

inline void Info_Display::expire_info_in(int timeout_ms)
{
    startTimer(timeout_ms);
}

inline void Info_Display::timerCallback()
{
    stopTimer();
    display_info_now(default_text_);
}
