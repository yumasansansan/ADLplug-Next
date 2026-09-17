// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// Runs a fuzz target on files, the way a libFuzzer build replays them, for the
// systems and builds that have no libFuzzer (plan D48). CTest gives it the seed
// inputs and the regression inputs of its target on every system.
//
//     <target>_replay <file or directory>...
//
// A directory stands for every file under it. The files run in the order of
// their paths, and each is named before it runs, so that a report of the
// sanitizers can be told apart by input. No input at all is a failure: it
// means the paths were wrong.

#include "fuzz.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

int main(int argc, char *argv[])
{
    namespace fs = std::filesystem;

    std::vector<fs::path> inputs;
    for (int i = 1; i < argc; ++i) {
        const fs::path path = argv[i];
        std::error_code error;
        if (fs::is_directory(path, error)) {
            for (const fs::directory_entry &entry : fs::recursive_directory_iterator(path)) {
                if (entry.is_regular_file())
                    inputs.push_back(entry.path());
            }
        }
        else if (fs::is_regular_file(path, error)) {
            inputs.push_back(path);
        }
        else {
            std::fprintf(stderr, "%s: no such file or directory\n", argv[i]);
            return 1;
        }
    }
    std::sort(inputs.begin(), inputs.end());

    if (inputs.empty()) {
        std::fprintf(stderr, "no inputs\n");
        return 1;
    }

    for (const fs::path &path : inputs) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) {
            std::fprintf(stderr, "%s: cannot be read\n", path.string().c_str());
            return 1;
        }
        const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        std::printf("%s (%zu bytes)\n", path.string().c_str(), bytes.size());
        std::fflush(stdout);
        LLVMFuzzerTestOneInput(bytes.data(), bytes.size());
    }

    std::printf("%zu inputs\n", inputs.size());
    return 0;
}
