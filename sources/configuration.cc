//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

#include "configuration.h"
#include "ui/utility/key_maps.h"
#include <SimpleIni.h>
#include <cstddef>
#include <cstdio>
#include <string>

#if 1
#   define trace(fmt, ...)
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
    MemoryBlock data;
    return file.existsAsFile() && file.loadFileAsData(data) &&
        ini.LoadData(static_cast<const char *>(data.getData()), data.getSize()) == SI_OK;
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

    if (!load_ini(ini_default->instance, system_file_path()))
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
    if (!load_ini(ini_user->instance, user))
        ini_ = std::move(ini_default);
    else {
        const long version = ini_user->instance.GetLongValue("", "configuration-version");
        if (version < config_version) {
            // use the latest configuration, keep a backup of the previous one
            ini_ = std::move(ini_default);
            user.moveFileTo(user.withFileExtension("ini.bak" + String(version)));
            save_default();
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
