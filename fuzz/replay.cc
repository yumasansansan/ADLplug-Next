// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// Runs a fuzz target on files, the way a libFuzzer build replays them, for the
// systems and builds that have no libFuzzer. CTest gives it the seed inputs
// and the regression inputs of its target on every system.
//
//     <target>_replay [--made <bytes>] <file or directory>...
//
// A directory stands for every file under it. The files run in the order of
// their paths, and each is named before it runs, so that a report of the
// sanitizers can be told apart by input. No input at all is a failure: it
// means the paths were wrong.
//
// --made makes up three inputs of that many bytes instead of reading them: every
// byte zero, every byte 0xff, and bytes from a generator with a seed of its own.
// A size may end in k, M or G. This is the only way a target is given an input of
// hundreds of megabytes, since libFuzzer keeps to a few kilobytes (-max_len) --
// and the size of what comes from outside is the plugin's to stand: a bank file
// is as large as it is, and a host hands over the state of a project whole.

#include "fuzz.h"
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <system_error>
#include <vector>

namespace {

// The size --made was given: bytes, which may end in k, M or G.
std::optional<std::size_t> made_size(const char *text) noexcept
{
    char *end = nullptr;
    errno = 0;
    const unsigned long long value = std::strtoull(text, &end, 10);
    if (errno != 0 || end == text || value == 0)
        return std::nullopt;

    std::size_t scale = 1;
    switch (*end) {
    case 'k': case 'K': scale = 1024; ++end; break;
    case 'M':           scale = std::size_t{1024} * 1024; ++end; break;
    case 'G':           scale = std::size_t{1024} * 1024 * 1024; ++end; break;
    default: break;
    }
    if (*end != '\0' || value > std::numeric_limits<std::size_t>::max() / scale)
        return std::nullopt;

    return static_cast<std::size_t>(value) * scale;
}

// The three made-up inputs, one after another, each named before it runs.
void run_made(std::size_t size)
{
    std::vector<std::uint8_t> bytes(size, 0);

    const auto run = [&bytes](const char *what) {
        std::printf("made: %s (%zu bytes)\n", what, bytes.size());
        std::fflush(stdout);
        LLVMFuzzerTestOneInput(bytes.data(), bytes.size());
    };

    run("every byte zero");

    std::ranges::fill(bytes, std::uint8_t{0xff});
    run("every byte 0xff");

    // A generator of its own, and a seed that does not change, so that an input
    // which fails can be had again.
    // The seed does not change on purpose: the input that a size makes up is the
    // same one every run, so that a failure of it can be looked at again.
    // NOLINTNEXTLINE(bugprone-random-generator-seed)
    std::minstd_rand generator(20260919u);
    for (std::uint8_t &byte : bytes)
        byte = static_cast<std::uint8_t>(generator() & 0xffu);
    run("bytes from a generator");
}

}  // namespace

int main(int argc, char *argv[])
{
    namespace fs = std::filesystem;

    std::optional<std::size_t> made;
    std::vector<fs::path> inputs;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];

        if (argument == "--made") {
            if (i + 1 == argc) {
                std::fprintf(stderr, "--made wants a number of bytes\n");
                return 1;
            }
            made = made_size(argv[++i]);
            if (!made) {
                std::fprintf(stderr, "%s: not a number of bytes\n", argv[i]);
                return 1;
            }
            continue;
        }

        const fs::path path = argument;
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

    if (inputs.empty() && !made) {
        std::fprintf(stderr, "no inputs\n");
        return 1;
    }

    if (made)
        run_made(*made);

    for (const fs::path &path : inputs) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) {
            std::fprintf(stderr, "%s: cannot be read\n", path.string().c_str());
            return 1;
        }
        std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        // libFuzzer hands a target a pointer even when the input has no bytes,
        // and a target may pass it on to something that wants one, memcpy for
        // instance; the data() of an empty vector is a null pointer. Ask for a
        // byte, so that there is something to point at.
        if (bytes.empty())
            bytes.reserve(1);
        std::printf("%s (%zu bytes)\n", path.string().c_str(), bytes.size());
        std::fflush(stdout);
        LLVMFuzzerTestOneInput(bytes.data(), bytes.size());
    }

    std::printf("%zu inputs\n", inputs.size() + (made ? 3u : 0u));
    return 0;
}
