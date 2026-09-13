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

class Algorithm_Component : public Component {
public:
    explicit Algorithm_Component(const char16_t *algorithm)
        : algorithm_(algorithm) {}

    void scale(double s);
    void paint(Graphics &g) override;

private:
    const char16_t *algorithm_ = nullptr;
    double scale_ = 24.0;
};
