# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
#   ci/flags.py <build directory>
#
# Lists the options of every command that a configured build runs with LLVM's
# compilers, linker, archivers and resource compiler, as Ninja runs them
# (ninja -t compdb -x, response files expanded), so that the log of a build
# shows every flag that reaches those tools, wherever it comes from: CMake,
# JUCE, the libraries or ADLplug-Next itself. Each option is counted over the
# commands of its kind, and the targets that carry it are named when not all
# of them do. Paths are left out: include and library directories, sources,
# objects and outputs. Definitions are listed apart, except those of JUCE's
# settings (JUCE_*, JucePlugin_*).
import collections
import json
import os
import re
import subprocess
import sys


def long_path(path):
    # CMake gives Windows programs whose paths have spaces by their short names.
    if os.name != 'nt' or '~' not in path:
        return path
    import ctypes
    buffer = ctypes.create_unicode_buffer(32768)
    length = ctypes.windll.kernel32.GetLongPathNameW(path, buffer, len(buffer))
    return buffer.value if 0 < length < len(buffer) else path


def program(token):
    name = re.split(r'[\\/]', long_path(token))[-1].lower()
    return name[:-4] if name.endswith('.exe') else name


KINDS = [
    ('compiler', re.compile(r'^clang(\+\+)?(-\d+)?$')),
    ('linker', re.compile(r'^(lld-link|ld\.lld|ld64\.lld)$')),
    ('archiver', re.compile(r'^llvm-(ar|ranlib)(-\d+)?$')),
    ('resource compiler', re.compile(r'^llvm-rc(-\d+)?$')),
]


def kind_of(token):
    if token.startswith(('-', '@')):
        return None
    name = program(token)
    return next((kind for kind, pattern in KINDS if pattern.match(name)), None)


TOKEN = re.compile(r'(?:[^\s"]+|"[^"]*")+')


def runs(command):
    """Yields the kind, the program and the arguments of each tool a command runs."""
    shell = re.match(r'^cmd(?:\.exe)?\s+/C\s+"(.*)"$', command, re.S | re.I)
    if shell:
        command = shell.group(1)
    for part in re.split(r'\s&&\s', command):
        tokens = [token.replace('"', '') for token in TOKEN.findall(part)]
        # A part can run several tools in turn: cmake -E cmake_llvm_rc runs the
        # preprocessor, then llvm-rc.
        starts = [i for i, token in enumerate(tokens) if kind_of(token)]
        for n, start in enumerate(starts):
            end = starts[n + 1] if n + 1 < len(starts) else len(tokens)
            yield kind_of(tokens[start]), program(tokens[start]), tokens[start + 1:end]


# Options whose value is the next argument: those left out, and those listed.
UNLISTED = {'-o', '-c', '-MF', '-MT', '-MQ', '-I', '-isystem', '-iquote', '-idirafter',
            '-imsvc', '-L', '-F', '/fo', '-fo'}
LISTED = {'-x', '-arch', '-target', '--target', '-isysroot', '--sysroot', '-include',
          '-framework', '-weak_framework', '-Xlinker', '-Xclang', '-Xassembler',
          '-Xpreprocessor', '-z', '-e', '-u', '-rpath', '-install_name', '-soname',
          '-lto_library', '-compatibility_version', '-current_version',
          '-exported_symbols_list', '-unexported_symbols_list'}
PATH = re.compile(r'[\\/]|\.(o|obj|a|lib|so|dylib|tbd|d|rsp|res|pp|rc|txt|json|c|cc|cpp|cxx|'
                  r'm|mm|h|hpp|pdb|exe|dll|def|exp|map|plist)$', re.I)
LANGUAGES = {'c': 'C', 'cc': 'C++', 'cpp': 'C++', 'cxx': 'C++', 'm': 'Objective-C',
             'mm': 'Objective-C++', 's': 'assembler', 'asm': 'assembler'}


def value(text):
    return '<path>' if PATH.search(text) else text


def option(text):
    if text.startswith('-Wl,'):
        parts = text.split(',')
        return ','.join([parts[0]] + [p if p.startswith('-') else value(p) for p in parts[1:]])
    joined = re.match(r'^([-/][^:=]*[:=])(.+)$', text)
    return joined.group(1) + value(joined.group(2)) if joined else text


def definition(text):
    name, equals, content = text.partition('=')
    if not equals:
        return name
    return f'{name}={content}' if re.match(r'^[\w.+-]{0,24}$', content) else f'{name}=...'


class Commands:
    def __init__(self):
        self.count = 0
        self.options = collections.Counter()
        self.definitions = collections.Counter()
        self.targets = collections.defaultdict(set)
        self.juce = set()


def record(groups, kind, name, arguments):
    slashes = kind == 'resource compiler' or name == 'lld-link'
    options, definitions = set(), set()
    output = source = None
    i = 0
    while i < len(arguments):
        argument = arguments[i]
        i += 1
        if argument == '--' or argument.startswith('@'):
            continue
        if not (argument.startswith('-') or (slashes and argument.startswith('/'))):
            continue
        if argument in UNLISTED or argument in LISTED or argument in ('-D', '-U'):
            following = arguments[i] if i < len(arguments) else ''
            i += 1
            if argument in ('-o', '/fo', '-fo'):
                output = following
            elif argument == '-c':
                source = following
            elif argument in ('-D', '-U'):
                definitions.add(('-U' if argument == '-U' else '') + following)
            elif argument in LISTED:
                shown = option(following) if argument.startswith('-X') else value(following)
                options.add(f'{argument} {shown}')
            continue
        if re.match(r'^-(I|isystem|iquote|idirafter|imsvc|L|F).', argument):
            continue
        if argument.startswith('-D'):
            definitions.add(argument[2:])
        elif argument.startswith('-U'):
            definitions.add(argument)
        elif re.match(r'^-l.', argument) or argument in ('-MD', '-MMD'):
            continue
        else:
            options.add(option(argument))

    if kind != 'compiler':
        group = kind
    elif '-E' in options:
        group = 'preprocessor, for resource scripts'
    elif source is not None:
        group = LANGUAGES.get(source.rsplit('.', 1)[-1].lower(), 'other') + ' compile'
    else:
        group = 'link'
    commands = groups[group]
    commands.count += 1
    target = '?'
    if output:
        directory = re.search(r'([^\\/]+)\.dir[\\/]', output)
        target = directory.group(1) if directory else re.split(r'[\\/]', output)[-1]
    for item in options:
        commands.options[item] += 1
        commands.targets[item].add(target)
    for item in definitions:
        if re.match(r'^(JUCE_|JucePlugin_)', item):
            commands.juce.add(item.partition('=')[0])
            continue
        shown = '-D ' + definition(item) if not item.startswith('-U') else item
        commands.definitions[shown] += 1
        commands.targets[shown].add(target)


def show(commands, counter):
    for item, count in sorted(counter.items(), key=lambda entry: (-entry[1], entry[0])):
        line = f'{count:7}  {item}'
        if count < commands.count:
            targets = sorted(commands.targets[item])
            line += '  (' + ', '.join(targets[:6])
            line += f' and {len(targets) - 6} more)' if len(targets) > 6 else ')'
        print(line)


def main():
    compdb = subprocess.run(['ninja', '-C', sys.argv[1], '-t', 'compdb', '-x'],
                            check=True, capture_output=True, text=True).stdout
    groups = collections.defaultdict(Commands)
    for entry in json.loads(compdb):
        for kind, name, arguments in runs(entry.get('command', '')):
            record(groups, kind, name, arguments)
    if not any(group.endswith(' compile') for group in groups):
        sys.exit(f'error: no compile commands in {sys.argv[1]}')
    for group in sorted(groups):
        commands = groups[group]
        print(f'-- {group}: {commands.count} commands')
        show(commands, commands.options)
        if commands.definitions or commands.juce:
            print(f'   definitions ({len(commands.juce)} of JUCE settings not listed)')
            show(commands, commands.definitions)


main()
