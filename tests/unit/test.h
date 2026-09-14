// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

#pragma once

// A small test registry, so the unit tests need no framework.
// ADLPLUG_TEST(name) defines a test, and CHECK(condition) records a failure
// and carries on. tests/CMakeLists.txt finds each ADLPLUG_TEST that starts a
// line and registers it with CTest as unit.<name>.

namespace Test {

struct Case {
    const char *name;
    void (*run)();
    Case *next;
};

void add(Case &test) noexcept;
void fail(const char *file, int line, const char *condition) noexcept;

struct Registrar {
    explicit Registrar(Case &test) noexcept
        { add(test); }
};

}  // namespace Test

#define ADLPLUG_TEST(name)                                                            \
    static void adlplug_test_##name();                                                \
    static Test::Case adlplug_case_##name {#name, &adlplug_test_##name, nullptr};     \
    static const Test::Registrar adlplug_registrar_##name {adlplug_case_##name};      \
    static void adlplug_test_##name()

// Variadic, so that a condition may contain commas, as in get<shift, size>().
#define CHECK(...)                                        \
    do {                                                  \
        if (!(__VA_ARGS__))                               \
            Test::fail(__FILE__, __LINE__, #__VA_ARGS__); \
    } while (false)
