#!/usr/bin/env python3
"""Pad DOL load sections to the 32-byte alignment required by strict loaders."""

import pathlib
import struct
import sys

HEADER_SIZE = 0x100
ALIGNMENT = 32
TEXT_OFFSETS = 0x00
DATA_OFFSETS = 0x1C
TEXT_SIZES = 0x90
DATA_SIZES = 0xAC


def read_u32(data, offset):
    return struct.unpack_from(">I", data, offset)[0]


def write_u32(data, offset, value):
    struct.pack_into(">I", data, offset, value)


def align(value):
    return (value + ALIGNMENT - 1) & -ALIGNMENT


def main(path):
    source = bytearray(path.read_bytes())
    if len(source) < HEADER_SIZE:
        raise ValueError("DOL is smaller than its header")

    sections = []
    for offsets_base, sizes_base, count in ((TEXT_OFFSETS, TEXT_SIZES, 7),
                                            (DATA_OFFSETS, DATA_SIZES, 11)):
        for index in range(count):
            offset_field = offsets_base + index * 4
            size_field = sizes_base + index * 4
            offset = read_u32(source, offset_field)
            size = read_u32(source, size_field)
            if not size:
                continue
            if offset < HEADER_SIZE or offset + size > len(source):
                raise ValueError(f"section {index} is outside DOL data")
            sections.append((offset, size, offset_field, size_field))

    sections.sort()
    output = bytearray(source[:HEADER_SIZE])
    for offset, size, offset_field, size_field in sections:
        new_offset = len(output)
        new_size = align(size)
        output.extend(source[offset:offset + size])
        output.extend(b"\0" * (new_size - size))
        write_u32(output, offset_field, new_offset)
        write_u32(output, size_field, new_size)

    path.write_bytes(output)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {sys.argv[0]} DOL")
    main(pathlib.Path(sys.argv[1]))
