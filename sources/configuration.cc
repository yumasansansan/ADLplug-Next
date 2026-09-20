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

#include "configuration.h"
#include "ui/utility/key_maps.h"
#include <SimpleIni.h>
#include <cstddef>
#include <cstdio>
#include <string>

#if 1
#   define trace(fmt, ...) ((void)0)
#else
#   define trace(fmt, ...) std::fprintf(stderr, "[Configuration] " fmt "\n" __VA_OPT__(,) __VA_ARGS__)
#endif

struct Configuration::Opaque_Ini {
    Opaque_Ini() { instance.SetUnicode(); }
    CSimpleIniA instance;
};

namespace {

constexpr long config_version = 1;

// The files go through JUCE because SimpleIni opens them by narrow path, which
// Windows takes in the ANSI code page: a user directory whose name has other
// characters could not be opened.
bool load_ini(CSimpleIniA &ini, const File &file)
{
    ini.Reset();
    MemoryBlock data;
    return file.existsAsFile() && file.loadFileAsData(data) &&
        ini.LoadData(data.begin(), data.getSize()) == SI_OK;
}

bool save_ini(const CSimpleIniA &ini, const File &file)
{
    std::string data;
    return ini.Save(data, false) == SI_OK &&
        file.getParentDirectory().createDirectory().wasOk() &&
        file.replaceWithData(data.data(), data.size());
}

void create_default_configuration(CSimpleIniA &ini)
{
    ini.Reset();

    ini.SetValue("", "configuration-version", std::to_string(config_version).c_str(),
                 "# the version of the file specification");

    ini.SetValue("paths", "last-instrument-directory", "",
                 "# the last directory in which instruments have been accessed");

    ini.SetValue("piano", "layout", name_of_key_layout(Key_Layout::Default),
                 "# the default key layout");

    for (std::size_t i = 0; i < key_layout_names.size(); ++i) {
        const std::string key = std::string("keymap:") + key_layout_names[i];
        const std::string comment = "# the " + String(key_layout_names[i]).toUpperCase().toStdString() + " key map";
        ini.SetValue("piano", key.c_str(), default_key_map(static_cast<Key_Layout>(i)).toRawUTF8(), comment.c_str());
    }
}

// Where upstream ADLplug, or OPNplug, keeps its configuration. ADLplug-Next
// starts from it when it has none of its own, and saves to its own.
File upstream_system_file_path()
{
#if defined(JUCE_LINUX)
    return File("/etc/" ADLPLUG_UPSTREAM_NAME "/" ADLPLUG_UPSTREAM_NAME ".ini");
#else
    return {};
#endif
}

File upstream_user_file_path()
{
    const File data_dir = File::getSpecialLocation(File::userApplicationDataDirectory);
    return data_dir.getChildFile(ADLPLUG_UPSTREAM_MANUFACTURER "/" ADLPLUG_UPSTREAM_NAME ".ini");
}

}  // namespace

Configuration::Configuration()
    : ini_(std::make_unique<Opaque_Ini>())
{
}

Configuration::~Configuration()
    = default;

File Configuration::system_file_path()
{
#if defined(JUCE_LINUX)
    return File("/etc/" JucePlugin_Name "/" JucePlugin_Name ".ini");
#else
    return {};
#endif
}

File Configuration::user_file_path()
{
    const File data_dir = File::getSpecialLocation(File::userApplicationDataDirectory);
    return data_dir.getChildFile(JucePlugin_Manufacturer "/" JucePlugin_Name ".ini");
}

void Configuration::load_default()
{
    auto ini_default = std::make_unique<Opaque_Ini>();

    if (!load_ini(ini_default->instance, system_file_path()) &&
        !load_ini(ini_default->instance, upstream_system_file_path()))
        create_default_configuration(ini_default->instance);
    else {
        const long version = ini_default->instance.GetLongValue("", "configuration-version");
        if (version < config_version) {
            std::fprintf(stderr, "!! " JucePlugin_Name " configuration: the system version (%ld) does not match the software version (%ld)!\n",
                         version, config_version);
            create_default_configuration(ini_default->instance);
        }
    }

    const File user = user_file_path();
    auto ini_user = std::make_unique<Opaque_Ini>();
    const bool own = load_ini(ini_user->instance, user);
    if (!own && !load_ini(ini_user->instance, upstream_user_file_path()))
        ini_ = std::move(ini_default);
    else {
        const long version = ini_user->instance.GetLongValue("", "configuration-version");
        if (version < config_version) {
            // use the latest configuration, keep a backup of the previous one;
            // the file of upstream stays as it is
            ini_ = std::move(ini_default);
            if (own) {
                user.moveFileTo(user.withFileExtension("ini.bak" + String(version)));
                save_default();
            }
        }
        else
            ini_ = std::move(ini_user);
    }
}

bool Configuration::save_default()
{
    const File user = user_file_path();
    trace("Attempt to save '%s'", user.getFullPathName().toRawUTF8());
    return save_file(user);
}

bool Configuration::load_file(const File &file)
{
    return load_ini(ini_->instance, file);
}

bool Configuration::save_file(const File &file)
{
    return save_ini(ini_->instance, file);
}

void Configuration::set_string(const char *section, const char *key, const char *value)
{
    ini_->instance.SetValue(section, key, value, nullptr, true);
}

const char *Configuration::get_string(const char *section, const char *key, const char *default_value) const
{
    return ini_->instance.GetValue(section, key, default_value);
}
