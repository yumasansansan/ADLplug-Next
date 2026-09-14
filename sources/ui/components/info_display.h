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
