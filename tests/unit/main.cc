//     Part of ADLplug, distributed under the GNU GPL v3 or later.
//               (See accompanying file LICENSE.)
//
//     ADLplug_unit_tests <name>    runs one test
//     ADLplug_unit_tests --list    lists the tests

#include "test.h"
#include <cstdio>
#include <cstring>

namespace Test {

namespace {

Case *first_case = nullptr;
int failures = 0;

}  // namespace

void add(Case &test) noexcept
{
    test.next = first_case;
    first_case = &test;
}

void fail(const char *file, int line, const char *condition) noexcept
{
    ++failures;
    std::fprintf(stderr, "%s:%d: check failed: %s\n", file, line, condition);
}

}  // namespace Test

int main(int argc, char *argv[])
{
    if (argc == 2 && std::strcmp(argv[1], "--list") == 0) {
        for (const Test::Case *test = Test::first_case; test != nullptr; test = test->next)
            std::printf("%s\n", test->name);
        return 0;
    }
    if (argc != 2) {
        std::fprintf(stderr, "usage: ADLplug_unit_tests <name> | --list\n");
        return 2;
    }

    for (const Test::Case *test = Test::first_case; test != nullptr; test = test->next) {
        if (std::strcmp(test->name, argv[1]) != 0)
            continue;
        test->run();
        if (Test::failures != 0) {
            std::fprintf(stderr, "%s: %d check(s) failed\n", test->name, Test::failures);
            return 1;
        }
        std::printf("%s: passed\n", test->name);
        return 0;
    }

    std::fprintf(stderr, "no test named %s\n", argv[1]);
    return 2;
}
