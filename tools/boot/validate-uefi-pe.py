#!/usr/bin/env python3
"""Fail closed unless an AArch64 UEFI application is firmware-relocatable."""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

IMAGE_FILE_MACHINE_ARM64 = 0xAA64
PE32_PLUS_MAGIC = 0x20B
EFI_APPLICATION_SUBSYSTEM = 10
BASE_RELOCATION_DIRECTORY = 5


def fail(message: str) -> None:
    raise SystemExit(f"error: {message}")


def read_u16(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 2 > len(data):
        fail("truncated PE/COFF image")
    return struct.unpack_from("<H", data, offset)[0]


def read_u32(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 4 > len(data):
        fail("truncated PE/COFF image")
    return struct.unpack_from("<I", data, offset)[0]


def read_u64(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 8 > len(data):
        fail("truncated PE/COFF image")
    return struct.unpack_from("<Q", data, offset)[0]


def validate(path: Path) -> None:
    data = path.read_bytes()
    if len(data) < 0x100 or data[:2] != b"MZ":
        fail(f"{path} is not a DOS/PE image")

    pe_offset = read_u32(data, 0x3C)
    if pe_offset + 24 > len(data) or data[pe_offset : pe_offset + 4] != b"PE\0\0":
        fail(f"{path} has no valid PE signature")

    coff = pe_offset + 4
    machine = read_u16(data, coff)
    section_count = read_u16(data, coff + 2)
    optional_size = read_u16(data, coff + 16)
    if machine != IMAGE_FILE_MACHINE_ARM64:
        fail(f"{path} machine is 0x{machine:04x}, expected ARM64 0xaa64")

    optional = coff + 20
    if optional + optional_size > len(data):
        fail(f"{path} has a truncated optional header")
    if read_u16(data, optional) != PE32_PLUS_MAGIC:
        fail(f"{path} is not PE32+")

    image_base = read_u64(data, optional + 24)
    subsystem = read_u16(data, optional + 68)
    if subsystem != EFI_APPLICATION_SUBSYSTEM:
        fail(f"{path} subsystem is {subsystem}, expected EFI application 10")

    directory_count = read_u32(data, optional + 108)
    if directory_count <= BASE_RELOCATION_DIRECTORY:
        fail(f"{path} has no PE base-relocation directory")
    reloc_rva = read_u32(data, optional + 112 + BASE_RELOCATION_DIRECTORY * 8)
    reloc_size = read_u32(data, optional + 116 + BASE_RELOCATION_DIRECTORY * 8)
    if reloc_rva == 0 or reloc_size < 8:
        fail(
            f"{path} is not relocatable: relocation directory is empty; "
            f"preferred ImageBase=0x{image_base:x}"
        )

    section_table = optional + optional_size
    reloc_section = False
    for index in range(section_count):
        offset = section_table + index * 40
        if offset + 40 > len(data):
            fail(f"{path} has a truncated section table")
        name = data[offset : offset + 8].split(b"\0", 1)[0]
        virtual_size = read_u32(data, offset + 8)
        virtual_address = read_u32(data, offset + 12)
        span = max(virtual_size, read_u32(data, offset + 16))
        if name == b".reloc" and virtual_address <= reloc_rva < virtual_address + span:
            reloc_section = True
            break
    if not reloc_section:
        fail(f"{path} relocation directory is not backed by a .reloc section")

    print(
        "AegisOS UEFI PE validation passed: "
        f"machine=ARM64 subsystem=EFI-application image-base=0x{image_base:x} "
        f"reloc-rva=0x{reloc_rva:x} reloc-size=0x{reloc_size:x}"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path)
    args = parser.parse_args()
    validate(args.image)


if __name__ == "__main__":
    main()
