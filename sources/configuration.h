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
#include <JuceHeader.h>
#include <memory>

class Configuration {
public:
    static File system_file_path();
    static File user_file_path();

    Configuration();
    ~Configuration();
    // Takes the user's configuration, or else the one of upstream ADLplug, or the
    // default one if there is none or it is of an older version.
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
