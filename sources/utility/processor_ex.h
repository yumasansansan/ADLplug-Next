//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

#pragma once
#include "parameter_ex.h"
#include "JuceHeader.h"

class AudioProcessorEx : public AudioProcessor,
                         public AudioParametersEx::ValueChangedListener {
public:
    using AudioProcessor::AudioProcessor;
    ~AudioProcessorEx() override = default;
};
