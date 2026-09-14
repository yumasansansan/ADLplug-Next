// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// Generates the pack of instrument banks that the plugin embeds
// (sources/resources.c), when the plugin is built:
//
//     ADLplug_bankgen --source <dir> [--list <banks.ini>] [--greyzone]
//                     --pak <banks.pak> [--notices <file.txt>] [--depfile <file.d>]
//
// <dir> is the source tree of ADLplug-Next, whose submodules in thirdparty/
// hold the banks. The OPL3 build takes libADLMIDI's list of the banks that it
// builds in: banks.ini with --greyzone, and otherwise banks-no-grey.ini, which
// puts placeholders in the places of the banks of libADLMIDI's grey zone, those
// made without an explicit permission of their authors. Several places share a
// placeholder there; the pack has each file once, under the name that banks.ini
// gives it too, if there is one. The OPN2 build takes the list given with
// --list (resources/opn2/banks.ini), in the same form, and leaves out the banks
// that it marks as grey zone unless --greyzone is given.
//
// Each bank goes into the pack as a WOPL or WOPN file, with a text on it: the
// file it comes from, and what its sources say of it (the BANK_INFO of a WOPLX
// bank, or the files that the OPN2 list names). The editor shows that text,
// --notices writes the texts of all the banks to one file, and --depfile lists
// the files read, for the build system. sources/utility/pak.h describes the
// pack.

#include "adl/wopx_file.h"
#if defined(ADLPLUG_OPL3)
#include "woplx.h"
#elif defined(ADLPLUG_OPN2)
#include "opn2_import.h"
#include "adl/instrument.h"
#include "adl/measurer.h"
#endif
#include <SimpleIni.h>
#include <juce_core/juce_core.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;

struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Options {
    fs::path source;
    fs::path list;
    fs::path pak;
    fs::path notices;
    fs::path depfile;
    bool greyzone = false;
};

constexpr const char usage[] =
    "usage: ADLplug_bankgen --source <dir> [--list <banks.ini>] [--greyzone]\n"
    "                       --pak <banks.pak> [--notices <file.txt>] [--depfile <file.d>]";

Options parse_options(int argc, char *argv[])
{
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        const auto next_path = [&]() {
            if (i + 1 == argc)
                throw Error(std::format("{} needs a value", arg));
            return fs::path(argv[++i]);
        };
        if (arg == "--source")
            options.source = next_path();
        else if (arg == "--list")
            options.list = next_path();
        else if (arg == "--pak")
            options.pak = next_path();
        else if (arg == "--notices")
            options.notices = next_path();
        else if (arg == "--depfile")
            options.depfile = next_path();
        else if (arg == "--greyzone")
            options.greyzone = true;
        else
            throw Error(std::format("unknown argument {}\n{}", arg, usage));
    }
    if (options.source.empty() || options.pak.empty())
        throw Error(usage);
    return options;
}

// Reads the files that the banks come from, and keeps their paths for the
// depfile.
class Inputs {
public:
    std::string read(const fs::path &path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
            throw Error(std::format("cannot open {}", path.generic_string()));
        std::ostringstream buffer;
        buffer << stream.rdbuf();
        if (stream.bad())
            throw Error(std::format("cannot read {}", path.generic_string()));
        paths_.insert(fs::absolute(path).lexically_normal());
        return std::move(buffer).str();
    }

    const std::set<fs::path> &paths() const noexcept
        { return paths_; }

private:
    std::set<fs::path> paths_;
};

// A bank as a list names it. The keys after `file` belong to the OPN2 list.
struct List_Entry {
    std::string section;
    std::string name;
    std::string games;
    std::string format;
    std::string file;
    std::string info;
    std::string license;
    std::string note;
    bool greyzone = false;
};

// Reads a list of banks in the form of libADLMIDI's banks.ini: the number of
// banks in [General], and a section bank-<n> for each, with the given keys
// only. Values may be quoted.
std::vector<List_Entry> read_list(Inputs &inputs, const fs::path &path, std::span<const std::string_view> keys)
{
    const std::string where = path.generic_string();
    const std::string text = inputs.read(path);
    CSimpleIniA ini;
    ini.SetUnicode(true);
    if (ini.LoadData(text.data(), text.size()) < 0)
        throw Error(std::format("{}: cannot be parsed", where));

    const long count = ini.GetLongValue("General", "banks", -1);
    if (count < 0)
        throw Error(std::format("{}: no number of banks", where));

    std::vector<List_Entry> entries;
    for (long n = 0; n < count; ++n) {
        List_Entry entry;
        entry.section = std::format("bank-{}", n);
        const char *section = entry.section.c_str();

        CSimpleIniA::TNamesDepend names;
        if (!ini.GetAllKeys(section, names))
            throw Error(std::format("{}: no section {}", where, entry.section));
        for (const CSimpleIniA::Entry &name : names) {
            if (std::find(keys.begin(), keys.end(), std::string_view(name.pItem)) == keys.end())
                throw Error(std::format("{}, {}: unknown key {}", where, entry.section, name.pItem));
        }

        const auto value = [&ini, section](const char *key) {
            std::string v = ini.GetValue(section, key, "");
            if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
                v = v.substr(1, v.size() - 2);
            return v;
        };
        entry.name = value("name");
        entry.games = value("games");
        entry.format = value("format");
        entry.file = value("file");
        entry.info = value("info");
        entry.license = value("license");
        entry.note = value("note");
        entry.greyzone = ini.GetBoolValue(section, "greyzone", false);
        if (entry.name.empty() || entry.format.empty() || entry.file.empty())
            throw Error(std::format("{}, {}: a bank needs a name, a format and a file", where, entry.section));
        entries.push_back(std::move(entry));
    }
    return entries;
}

// libADLMIDI names a bank "AIL (Warcraft 2)", with notes such as ":MT-32:"
// after the bracket. ADLplug has named its banks "[AIL] Warcraft 2" since its
// first pack, and keeps the notes.
std::string display_name(const std::string &name)
{
    const std::size_t open = name.find(" (");
    const std::size_t close = name.rfind(')');
    if (open == std::string::npos || open == 0 || close == std::string::npos || close < open ||
        name.find(' ') < open)
        return name;
    return "[" + name.substr(0, open) + "] " + name.substr(open + 2, close - open - 2) + name.substr(close + 1);
}

// A text for the pack: valid UTF-8, lines ending in LF, no blank lines around.
std::string plain_text(std::string_view text, const std::string &where)
{
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        !juce::CharPointer_UTF8::isValidString(text.data(), static_cast<int>(text.size())))
        throw Error(std::format("{}: the text is not UTF-8", where));

    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (!(text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n'))
            out += text[i];
    }
    out.erase(0, std::min(out.find_first_not_of('\n'), out.size()));
    const std::size_t last = out.find_last_not_of(" \t\n");
    out.erase((last == std::string::npos) ? 0 : last + 1);
    if (!out.empty())
        out += '\n';
    return out;
}

// A line that the tool writes, wrapped at 79 columns like most texts of the
// banks. Each line of the input is wrapped on its own.
std::string wrapped(std::string_view text)
{
    constexpr std::size_t width = 79;
    std::string out;
    for (std::size_t line_start = 0; line_start < text.size();) {
        std::size_t line_end = text.find('\n', line_start);
        if (line_end == std::string_view::npos)
            line_end = text.size();
        std::size_t column = 0;
        for (std::size_t pos = line_start; pos < line_end;) {
            std::size_t word_end = text.find(' ', pos);
            if (word_end == std::string_view::npos || word_end > line_end)
                word_end = line_end;
            const std::string_view word = text.substr(pos, word_end - pos);
            pos = word_end + 1;
            if (word.empty())
                continue;
            if (column != 0 && column + 1 + word.size() > width) {
                out += '\n';
                column = 0;
            }
            else if (column != 0) {
                out += ' ';
                ++column;
            }
            out += word;
            column += word.size();
        }
        out += '\n';
        line_start = line_end + 1;
    }
    return out;
}

// Saves a bank, and checks that it loads back as it was.
std::string saved_bank(WOPx::BankFile &file, std::uint16_t version, const std::string &where)
{
    const std::size_t size = WOPx::CalculateBankFileSize(&file, version);
    std::string data(size, '\0');
    if (size == 0 || WOPx::SaveBankToMem(&file, data.data(), size, version, 0) != 0)
        throw Error(std::format("{}: the bank cannot be saved", where));
    int error = 0;
    const WOPx::BankFile_Ptr loaded(WOPx::LoadBankFromMem(data.data(), size, &error));
    if (!loaded || WOPx::BanksCmp(&file, loaded.get()) != 1)
        throw Error(std::format("{}: the saved bank does not load back as it was (error {})", where, error));
    return data;
}

struct Pack_Entry {
    std::string name;
    std::string data;
    std::string info;
};

#if defined(ADLPLUG_OPL3)

// The keys of libADLMIDI's lists. Those after "file" matter to gen_adldata for
// formats other than WOPLX, whose file holds all of that itself.
constexpr std::string_view list_keys[] {
    "name", "games", "format", "file", "file-p", "prefix", "prefix-p",
    "filter-m", "filter-p", "no-rhythm-mode", "mt32-defaults",
};

// Keeps each file once, for a list in which several banks share one: the first
// bank that `reference` has as it is, with the same name and file, or else the
// first bank.
std::vector<List_Entry> without_repeated_files(const std::vector<List_Entry> &list,
                                               const std::vector<List_Entry> &reference)
{
    const auto in_reference = [&reference](const List_Entry &entry) {
        return std::any_of(reference.begin(), reference.end(), [&entry](const List_Entry &r) {
            return r.name == entry.name && r.file == entry.file;
        });
    };

    std::map<std::string, std::size_t> kept;
    for (std::size_t i = 0; i < list.size(); ++i) {
        const auto [it, inserted] = kept.try_emplace(list[i].file, i);
        if (!inserted && !in_reference(list[it->second]) && in_reference(list[i]))
            it->second = i;
    }

    std::vector<List_Entry> result;
    for (std::size_t i = 0; i < list.size(); ++i) {
        if (kept.at(list[i].file) == i)
            result.push_back(list[i]);
    }
    return result;
}

std::vector<Pack_Entry> read_banks(Inputs &inputs, const Options &options)
{
    const fs::path library = options.source / "thirdparty" / "libADLMIDI";
    std::vector<List_Entry> list = read_list(inputs, library / "banks.ini", list_keys);
    if (!options.greyzone)
        list = without_repeated_files(read_list(inputs, library / "banks-no-grey.ini", list_keys), list);

    std::vector<Pack_Entry> entries;
    for (const List_Entry &item : list) {
        const std::string where = "thirdparty/libADLMIDI/" + item.file;
        if (item.format != "WOPLX")
            throw Error(std::format("{}: the format {} is not supported", where, item.format));

        Woplx_Bank bank;
        std::string error;
        if (!read_woplx(inputs.read(library / item.file), bank, error))
            throw Error(std::format("{}: {}", where, error));

        Pack_Entry entry;
        entry.name = display_name(item.name);
        entry.data = saved_bank(*bank.file, 3, where);
        if (!item.games.empty())
            entry.info += wrapped(std::format("Games: {}", item.games));
        entry.info += wrapped(std::format("File: {}", where));
        if (item.file.starts_with("fm_banks_new/greyzone/"))
            entry.info += wrapped("Grey zone: libADLMIDI keeps this bank among those made without an explicit "
                                  "permission of their authors, whose legal status is unclear "
                                  "(fm_banks_new/greyzone/README.txt).");
        if (const std::string text = plain_text(bank.info, where); !text.empty())
            entry.info += "\n" + text;
        entries.push_back(std::move(entry));
    }
    return entries;
}

#elif defined(ADLPLUG_OPN2)

constexpr std::string_view list_keys[] {
    "name", "format", "file", "info", "license", "note", "greyzone",
};

std::uint16_t milliseconds(std::uint64_t ms)
{
    return static_cast<std::uint16_t>(std::min<std::uint64_t>(ms, std::numeric_limits<std::uint16_t>::max()));
}

// Measures how long each instrument sounds, as the plugin does with the
// instruments that it edits (Worker::measure()). WOPN banks keep these
// durations, and take an instrument without any for a blank one. Returns the
// number of instruments that make no sound.
unsigned measure(WOPNFile &file)
{
    unsigned silent = 0;
    const auto measure_bank = [&silent](WOPNBank &bank) {
        for (WOPNInstrument &ins : bank.ins) {
            if ((ins.inst_flags & WOPN_Ins_IsBlank) != 0)
                continue;
            Measurer::DurationInfo result;
            Measurer::ComputeDurations(Instrument::from_wopl(ins), result);
            ins.delay_on_ms = milliseconds(result.ms_sound_kon);
            ins.delay_off_ms = milliseconds(result.ms_sound_koff);
            if (ins.delay_on_ms == 0 && ins.delay_off_ms == 0) {
                ins.inst_flags |= WOPN_Ins_IsBlank;
                ++silent;
            }
        }
    };
    for (std::size_t i = 0; i < file.banks_count_melodic; ++i)
        measure_bank(file.banks_melodic[i]);
    for (std::size_t i = 0; i < file.banks_count_percussion; ++i)
        measure_bank(file.banks_percussive[i]);
    return silent;
}

std::vector<Pack_Entry> read_banks(Inputs &inputs, const Options &options)
{
    if (options.list.empty())
        throw Error("the OPN2 banks need --list");
    const std::vector<List_Entry> list = read_list(inputs, options.list, list_keys);

    std::vector<Pack_Entry> entries;
    for (const List_Entry &item : list) {
        if (item.greyzone && !options.greyzone)
            continue;
        const std::string &where = item.file;
        std::string data = inputs.read(options.source / item.file);

        Pack_Entry entry;
        entry.name = display_name(item.name);
        std::string notes;
        if (item.format == "WOPN") {
            // Taken as it is, once it is known to load.
            std::string copy = data;
            int error = 0;
            const WOPNFile_Ptr file(WOPN_LoadBankFromMem(copy.data(), copy.size(), &error));
            if (!file)
                throw Error(std::format("{}: not a WOPN bank (error {})", where, error));
            entry.data = std::move(data);
        }
        else if (item.format == "GYB" || item.format == "GEMS") {
            const std::span<const std::uint8_t> bytes(reinterpret_cast<const std::uint8_t *>(data.data()), data.size());
            Imported_Bank bank;
            std::string error;
            const bool imported = (item.format == "GYB") ? import_gyb(bytes, bank, error) : import_gems(bytes, bank, error);
            if (!imported)
                throw Error(std::format("{}: {}", where, error));
            notes = bank.notes;
            if (const unsigned silent = measure(*bank.file); silent != 0)
                notes += std::format("{} instruments make no sound, and are left blank.\n", silent);
            entry.data = saved_bank(*bank.file, 2, where);
        }
        else
            throw Error(std::format("{}: the format {} is not supported", where, item.format));

        entry.info = wrapped(std::format("File: {}", where));
        if (item.greyzone)
            entry.info += wrapped("Grey zone: no license or permission of its authors is known for this bank.");
        if (!item.note.empty())
            entry.info += wrapped(item.note);
        entry.info += wrapped(notes);
        for (const std::string &text_file : {item.info, item.license}) {
            if (text_file.empty())
                continue;
            entry.info += std::format("\nFrom {}:\n\n", text_file);
            entry.info += plain_text(inputs.read(options.source / text_file), text_file);
        }
        entries.push_back(std::move(entry));
    }
    return entries;
}

#endif

// The editor finds the text of the bank it has loaded by the bank's title,
// which holds 64 bytes (AdlplugAudioProcessor::bank_title_size_max): the names
// have to fit in it whole, and to differ.
void check_entries(const std::vector<Pack_Entry> &entries)
{
    if (entries.empty())
        throw Error("no banks");
    std::set<std::string> names;
    for (const Pack_Entry &entry : entries) {
        if (entry.name.empty() || entry.name.size() > 64 || entry.name.find('\0') != std::string::npos ||
            !juce::CharPointer_UTF8::isValidString(entry.name.data(), static_cast<int>(entry.name.size())))
            throw Error(std::format("the name \"{}\" does not fit a bank title", entry.name));
        if (!names.insert(entry.name).second)
            throw Error(std::format("two banks are named \"{}\"", entry.name));
        if (entry.data.empty())
            throw Error(std::format("the bank \"{}\" is empty", entry.name));
    }
}

void put_u32(std::string &out, std::size_t value)
{
    if (value > std::numeric_limits<std::uint32_t>::max())
        throw Error("the pack is too large");
    for (int shift = 24; shift >= 0; shift -= 8)
        out += static_cast<char>((value >> shift) & 0xff);
}

std::string pack(const std::vector<Pack_Entry> &entries)
{
    std::string dictionary = "PAK2";
    std::string content;
    for (const Pack_Entry &entry : entries) {
        const std::size_t data_offset = content.size();
        content += entry.data;
        const std::size_t info_offset = content.size();
        content += entry.info;
        put_u32(dictionary, entry.data.size());
        put_u32(dictionary, data_offset);
        put_u32(dictionary, entry.info.size());
        put_u32(dictionary, info_offset);
        dictionary += entry.name;
        dictionary += '\0';
    }
    put_u32(dictionary, 0);
    if (content.size() > std::numeric_limits<std::uint32_t>::max())
        throw Error("the pack is too large");

    juce::MemoryOutputStream compressed;
    {
        // With window bits 0, JUCE writes a zlib stream, whose header holds no
        // time or system: the same banks give the same pack everywhere.
        juce::GZIPCompressorOutputStream stream(compressed, 9, 0);
        if (!stream.write(content.data(), content.size()))
            throw Error("the pack cannot be compressed");
        stream.flush();
    }
    dictionary.append(static_cast<const char *>(compressed.getData()), compressed.getDataSize());
    return dictionary;
}

std::string notices(const std::vector<Pack_Entry> &entries, bool greyzone)
{
    std::string text =
        "The instrument banks of the plugin\n"
        "==================================\n"
        "\n"
        "The plugin embeds these banks, which tools/bankgen took from the submodules of\n"
        "ADLplug-Next's source tree when the plugin was built. For each bank, this file\n"
        "gives the file it comes from and what the sources say of the bank, with the\n"
        "terms of use where they give them. The editor shows the same text under \"Bank\n"
        "information...\".\n"
        "\n";
    text += greyzone ?
        "This build includes the banks of the grey zone, for which no license or\n"
        "permission of their authors is known.\n" :
        "This build leaves out the banks of the grey zone, for which no license or\n"
        "permission of their authors is known.\n";
    // The texts have rules of their own, of dashes and of equals signs.
    const std::string rule(80, '_');
    for (std::size_t i = 0; i < entries.size(); ++i)
        text += std::format("\n{}\n\nBank {} of {}: {}\n\n{}", rule, i + 1, entries.size(), entries[i].name, entries[i].info);
    return text;
}

// A Makefile rule that names the pack and the files that it was made from.
std::string depfile(const fs::path &target, const std::set<fs::path> &inputs)
{
    const auto escaped = [](const fs::path &path) {
        std::string out;
        for (const char c : fs::absolute(path).lexically_normal().generic_string()) {
            if (c == ' ' || c == '#' || c == '\\')
                out += '\\';
            else if (c == '$')
                out += '$';
            out += c;
        }
        return out;
    };
    std::string text = escaped(target) + ":";
    for (const fs::path &input : inputs)
        text += " \\\n  " + escaped(input);
    return text + "\n";
}

void write_file(const fs::path &path, std::string_view data)
{
    if (path.has_parent_path())
        fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(data.data(), static_cast<std::streamsize>(data.size()));
    stream.close();
    if (!stream)
        throw Error(std::format("cannot write {}", path.generic_string()));
}

}  // namespace

int main(int argc, char *argv[])
{
    try {
        const Options options = parse_options(argc, argv);
        Inputs inputs;
        const std::vector<Pack_Entry> entries = read_banks(inputs, options);
        check_entries(entries);
        const std::string pak = pack(entries);

        // Nothing is written unless every bank could be read.
        write_file(options.pak, pak);
        if (!options.notices.empty())
            write_file(options.notices, notices(entries, options.greyzone));
        if (!options.depfile.empty())
            write_file(options.depfile, depfile(options.pak, inputs.paths()));

        std::printf("ADLplug_bankgen: %zu banks%s, %zu bytes\n", entries.size(),
                    options.greyzone ? " with the grey zone" : "", pak.size());
        return 0;
    }
    catch (const std::exception &error) {
        std::fprintf(stderr, "ADLplug_bankgen: %s\n", error.what());
        return 1;
    }
}
