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
class Waves;

class Wave_Label : public Component,
                   public AsyncUpdater
{
public:
    explicit Wave_Label(const Waves &waves);
    explicit Wave_Label(const Waves &waves, const String &name);

    [[nodiscard]] unsigned wave() const
        { return wave_; }
    void set_wave(unsigned wave, NotificationType notification);

    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void wave_changed([[maybe_unused]] Wave_Label *k) {}
    };

    void add_listener(Listener *l);
    void remove_listener(Listener *l);

protected:
    void handleAsyncUpdate() override;
    void paint(Graphics &g) override;

private:
    const Waves &waves_;
    unsigned wave_ = 0;
    ListenerList<Listener> listeners_;
};
