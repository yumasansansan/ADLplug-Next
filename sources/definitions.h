//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

#pragma once

// bank slots to reserve in the synthesizer
inline constexpr unsigned bank_reserve_size = 64;

// maximum program notifications in a cycle
inline constexpr unsigned max_program_notifications = 32;

// maximum program measurement requests in a cycle
inline constexpr unsigned max_program_measurements = 32;

// maximum interval between midi processing cycles
inline constexpr unsigned midi_interval_max = 256;
