#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
#   tools/rename-font.py <font.ttf> <family>
#
# Gives a TrueType font a new family name, in place. The SIL Open Font License
# lets a font reserve its name for the original: a modified version, such as
# the subsets that tools/make-fonts.sh makes of the Liberation fonts, has to be
# called something else.
#
# The family, full, unique and PostScript names are rewritten (name IDs 1, 3, 4
# and 6, and 16, 17, 18 and 21 when the font has them); the copyright notice,
# the style and the version stay. The name must not remain in any other record.
# Every table checksum and the font's checksum adjustment are recomputed. Only
# Python's standard library is needed.

import re
import struct
import sys


def checksum(data):
    data += b"\0" * (-len(data) % 4)
    return sum(struct.unpack(f">{len(data) // 4}I", data)) & 0xFFFFFFFF


def read_tables(font):
    count = struct.unpack(">H", font[4:6])[0]
    tables = []
    for i in range(count):
        tag, _, offset, length = struct.unpack(">4sIII", font[12 + 16 * i:28 + 16 * i])
        tables.append((tag, offset, length))
    return tables


def decode(platform, encoding, raw):
    if platform in (0, 3):
        return raw.decode("utf-16-be")
    if platform == 1 and encoding == 0:
        return raw.decode("mac_roman")
    return None


def encode(platform, text):
    return text.encode("utf-16-be" if platform in (0, 3) else "mac_roman")


def read_names(table):
    form, count, storage = struct.unpack(">HHH", table[:6])
    if form != 0:
        sys.exit(f"name table format {form} is not supported")
    records = []
    for i in range(count):
        platform, encoding, language, name_id, length, offset = struct.unpack(
            ">6H", table[6 + 12 * i:18 + 12 * i])
        raw = table[storage + offset:storage + offset + length]
        records.append([platform, encoding, language, name_id, raw])
    return records


def write_names(records):
    records.sort(key=lambda record: record[:4])
    header = bytearray(struct.pack(">HHH", 0, len(records), 6 + 12 * len(records)))
    storage = bytearray()
    offsets = {}
    for platform, encoding, language, name_id, raw in records:
        if raw not in offsets:
            offsets[raw] = len(storage)
            storage += raw
        header += struct.pack(">6H", platform, encoding, language, name_id, len(raw), offsets[raw])
    return bytes(header + storage)


def rename(records, family):
    texts = {}
    for platform, encoding, _, name_id, raw in records:
        text = decode(platform, encoding, raw)
        if text is not None:
            texts.setdefault(name_id, text)
    old_family = texts.get(16, texts.get(1))
    old_postscript = texts.get(6)
    if not old_family or not old_postscript:
        sys.exit("the font has no family name or no PostScript name")
    old_postscript_family = old_postscript.split("-")[0]
    postscript_family = re.sub(r"[^A-Za-z0-9]", "", family)
    full = texts.get(4, old_family).replace(old_family, family)
    version = texts.get(5, "")

    for record in records:
        platform, encoding, _, name_id, raw = record
        text = decode(platform, encoding, raw)
        if text is None:
            sys.exit(f"name ID {name_id}: platform {platform}, encoding {encoding} is not supported")
        if name_id in (1, 4, 16, 18, 21):
            new = text.replace(old_family, family)
        elif name_id == 6:
            new = postscript_family + text[len(old_postscript_family):]
        elif name_id == 3:
            new = f"{full}: {version}" if version else full
        else:
            new = text
        if name_id not in (0, 7, 13, 14) and (old_family in new or old_postscript_family in new):
            sys.exit(f"name ID {name_id} still holds the old name: {new!r}")
        record[4] = encode(platform, new)


def rebuild(font, tables, replaced):
    data = {tag: font[offset:offset + length] for tag, offset, length in tables}
    data.update(replaced)
    head = bytearray(data[b"head"])
    head[8:12] = bytes(4)
    data[b"head"] = bytes(head)

    directory_size = 12 + 16 * len(tables)
    layout = {}
    body = bytearray()
    for tag, _, _ in sorted(tables, key=lambda table: table[1]):
        layout[tag] = directory_size + len(body)
        body += data[tag] + bytes(-len(data[tag]) % 4)

    out = bytearray(font[:12])
    for tag, _, _ in sorted(tables):
        out += struct.pack(">4sIII", tag, checksum(data[tag]), layout[tag], len(data[tag]))
    out += body
    adjustment = (0xB1B0AFBA - checksum(bytes(out))) & 0xFFFFFFFF
    out[layout[b"head"] + 8:layout[b"head"] + 12] = struct.pack(">I", adjustment)
    return bytes(out)


def verify(font):
    for tag, offset, length in read_tables(font):
        table = font[offset:offset + length]
        if tag == b"head":
            table = table[:8] + bytes(4) + table[12:]
        expected = struct.unpack(">I", font[12 + 16 * [t for t, _, _ in read_tables(font)].index(tag) + 4:][:4])[0]
        if checksum(table) != expected:
            sys.exit(f"checksum of table {tag.decode()} is wrong")
    if checksum(font) != 0xB1B0AFBA:
        sys.exit("the font's checksum adjustment is wrong")


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: rename-font.py <font.ttf> <family>")
    path, family = sys.argv[1:]
    font = open(path, "rb").read()
    if font[:4] not in (b"\0\1\0\0", b"true"):
        sys.exit(f"{path} is not a TrueType font")
    tables = read_tables(font)
    name = next((font[offset:offset + length] for tag, offset, length in tables if tag == b"name"), None)
    if name is None:
        sys.exit(f"{path} has no name table")
    records = read_names(name)
    rename(records, family)
    renamed = rebuild(font, tables, {b"name": write_names(records)})
    verify(renamed)
    open(path, "wb").write(renamed)


if __name__ == "__main__":
    main()
