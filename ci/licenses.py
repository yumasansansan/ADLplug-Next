#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
#   python3 ci/licenses.py <build directory> <directory>
#
# Gathers into <directory> the licenses that go with the binaries of a build:
# LICENSE and LICENSES/ of ADLplug-Next, and the license files of the
# third-party code that the plugins and the standalone program were built
# from. That code is found in what Ninja recorded as the sources and headers of
# the objects those targets link, under thirdparty/: each such file brings the
# license files of the nearest directory above it that has any, and those are
# copied at their paths in the source tree. README.txt says what is where. Run
# from the top of the source tree, after the build.

import re
import shutil
import subprocess
import sys
from pathlib import Path

# The targets whose binaries are distributed; a configuration builds some.
TARGETS = ["ADLplug_VST3", "ADLplug_LV2", "ADLplug_AU", "ADLplug_AAX", "ADLplug_Standalone"]
LICENSE_FILE = re.compile(r"^(LICEN[CS]E|COPYING)([-._].*)?$", re.IGNORECASE)

README = """\
The licenses that go with these binaries of ADLplug-Next and OPNplug-Next.

LICENSE
  The GNU General Public License, version 3, under which the binaries are
  distributed. They use JUCE under the GNU Affero General Public License,
  version 3, and the ASIO and AAX SDKs that come with JUCE under version 3 of
  the GPL.

LICENSES/
  The texts of the licenses that the files of ADLplug-Next's source tree name,
  in the form of the REUSE specification.

thirdparty/
  The license files of the third-party code that the binaries were built from,
  at their paths in the source tree.

The instrument banks keep their own terms, which <plugin>-banks.txt beside
this directory gives. The source code, with the third-party code in
thirdparty/, is at https://github.com/yumasansansan/ADLplug-Next.
"""


def ninja(build, *args, check=True):
    return subprocess.run(["ninja", "-C", str(build), *args], check=check,
                          capture_output=True, text=True)


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: licenses.py <build directory> <directory>")
    build = Path(sys.argv[1]).resolve()
    out = Path(sys.argv[2])
    top = Path.cwd().resolve()
    thirdparty = top / "thirdparty"

    # The objects that the distributed targets link, directly or through the
    # libraries they link.
    objects = set()
    for target in TARGETS:
        result = ninja(build, "-t", "inputs", target, check=False)
        if result.returncode != 0:
            continue
        for line in result.stdout.splitlines():
            name = line.strip().replace("\\", "/")
            if name.endswith((".o", ".obj")):
                objects.add(name)
    if not objects:
        sys.exit(f"error: {build} has no objects of {', '.join(TARGETS)}")

    # The sources and headers of those objects, from Ninja's dependency log.
    files = set()
    current = False
    for line in ninja(build, "-t", "deps").stdout.splitlines():
        if not line.strip():
            current = False
        elif not line[0].isspace():
            current = line.split(": #deps", 1)[0].replace("\\", "/") in objects
        elif current:
            files.add(line.strip())

    found = {}

    def licenses_of(directory):
        if directory not in found:
            here = []
            if directory.is_dir():
                here = sorted(p for p in directory.iterdir()
                              if p.is_file() and LICENSE_FILE.match(p.name))
            if not here and thirdparty in directory.parents:
                here = licenses_of(directory.parent)
            found[directory] = here
        return found[directory]

    chosen = set()
    for name in files:
        path = Path(name)
        if not path.is_absolute():
            path = build / path
        path = path.resolve()
        if thirdparty in path.parents:
            chosen.update(licenses_of(path.parent))

    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    shutil.copy2(top / "LICENSE", out / "LICENSE")
    shutil.copytree(top / "LICENSES", out / "LICENSES")
    for license_file in sorted(chosen):
        destination = out / license_file.relative_to(top)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(license_file, destination)
    (out / "README.txt").write_text(README, encoding="utf-8", newline="\n")

    print(f"{len(objects)} objects, {len(files)} files, {len(chosen)} third-party license files:")
    for license_file in sorted(chosen):
        print("  " + license_file.relative_to(top).as_posix())


if __name__ == "__main__":
    main()
