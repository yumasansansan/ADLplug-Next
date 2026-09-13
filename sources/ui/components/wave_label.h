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
class Waves;

class Wave_Label : public Component,
                   public AsyncUpdater
{
public:
    explicit Wave_Label(const Waves &waves);
    explicit Wave_Label(const Waves &waves, const String &name);

    unsigned wave() const
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
