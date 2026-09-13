//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

#pragma once
#include <JuceHeader.h>
#include <memory>

class Configuration {
public:
    static File system_file_path();
    static File user_file_path();

    Configuration();
    ~Configuration();
    // Takes the user's configuration, or the default one if the user has none
    // or has one of an older version.
    void load_default();
    bool save_default();
    bool load_file(const File &file);
    bool save_file(const File &file);

    void set_string(const char *section, const char *key, const char *value);
    const char *get_string(const char *section, const char *key, const char *default_value) const;

private:
    struct Opaque_Ini;
    std::unique_ptr<Opaque_Ini> ini_;
};
