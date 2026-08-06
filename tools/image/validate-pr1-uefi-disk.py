#!/usr/bin/env python3
"""Validate the PR1 GPT/FAT32 boot image without mounting it.

This catches a missing ESP, corrupt GPT/FAT metadata, wrong removable-media
path, or stale EFI/kernel payload before QEMU is started.
"""
from __future__ import annotations

import argparse
import binascii
import hashlib
import struct
import uuid
from dataclasses import dataclass
from pathlib import Path

SECTOR = 512
ESP_GUID = uuid.UUID("c12a7328-f81f-11d2-ba4b-00a0c93ec93b")


def fail(message: str) -> None:
    raise SystemExit(f"error: {message}")


def u16(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 2 > len(data):
        fail("truncated image")
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 4 > len(data):
        fail("truncated image")
    return struct.unpack_from("<I", data, offset)[0]


def u64(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 8 > len(data):
        fail("truncated image")
    return struct.unpack_from("<Q", data, offset)[0]


@dataclass(frozen=True)
class DirEntry:
    name: str
    attributes: int
    first_cluster: int
    size: int


class Fat32:
    def __init__(self, image: bytes, partition_lba: int, partition_sectors: int) -> None:
        self.image = image
        self.base = partition_lba * SECTOR
        self.limit = self.base + partition_sectors * SECTOR
        if self.limit > len(image):
            fail("ESP extends beyond the disk image")
        boot = image[self.base : self.base + SECTOR]
        if len(boot) != SECTOR or boot[510:512] != b"\x55\xaa":
            fail("ESP has no valid FAT boot-sector signature")

        self.bytes_per_sector = u16(boot, 11)
        self.sectors_per_cluster = boot[13]
        self.reserved_sectors = u16(boot, 14)
        self.fat_count = boot[16]
        total16 = u16(boot, 19)
        total32 = u32(boot, 32)
        self.total_sectors = total16 or total32
        self.fat_sectors = u32(boot, 36)
        self.root_cluster = u32(boot, 44)
        hidden = u32(boot, 28)

        if self.bytes_per_sector != SECTOR:
            fail(f"ESP bytes-per-sector is {self.bytes_per_sector}, expected {SECTOR}")
        if self.sectors_per_cluster == 0 or self.reserved_sectors == 0:
            fail("ESP has invalid FAT32 geometry")
        if self.fat_count < 1 or self.fat_sectors == 0:
            fail("ESP has no FAT")
        if self.total_sectors > partition_sectors:
            fail("FAT volume exceeds the ESP partition")
        if hidden != partition_lba:
            fail(f"FAT hidden-sector count is {hidden}, expected ESP LBA {partition_lba}")
        if boot[82:90] != b"FAT32   ":
            fail("ESP is not marked FAT32")
        if self.root_cluster < 2:
            fail("FAT32 root cluster is invalid")

        self.fat_offset = self.base + self.reserved_sectors * SECTOR
        self.data_sector = self.reserved_sectors + self.fat_count * self.fat_sectors
        if self.data_sector >= self.total_sectors:
            fail("FAT32 data area is empty")

    @property
    def cluster_bytes(self) -> int:
        return self.sectors_per_cluster * SECTOR

    def fat_value(self, cluster: int) -> int:
        offset = self.fat_offset + cluster * 4
        if offset + 4 > self.limit:
            fail("FAT chain points outside the ESP")
        return u32(self.image, offset) & 0x0FFFFFFF

    def cluster_offset(self, cluster: int) -> int:
        if cluster < 2:
            fail(f"invalid FAT cluster {cluster}")
        sector = self.data_sector + (cluster - 2) * self.sectors_per_cluster
        offset = self.base + sector * SECTOR
        if offset < self.base or offset + self.cluster_bytes > self.limit:
            fail(f"FAT cluster {cluster} points outside the ESP")
        return offset

    def chain(self, first_cluster: int) -> list[int]:
        chain: list[int] = []
        seen: set[int] = set()
        cluster = first_cluster
        while True:
            if cluster in seen:
                fail("FAT cluster chain contains a loop")
            if cluster < 2 or cluster >= 0x0FFFFFF0:
                fail(f"invalid FAT cluster in chain: 0x{cluster:x}")
            seen.add(cluster)
            chain.append(cluster)
            if len(chain) > 1_000_000:
                fail("FAT cluster chain is unreasonably long")
            nxt = self.fat_value(cluster)
            if nxt >= 0x0FFFFFF8:
                return chain
            if nxt == 0:
                fail("FAT cluster chain ends in a free cluster")
            if 0x0FFFFFF0 <= nxt < 0x0FFFFFF8:
                fail(f"FAT cluster chain ends in reserved/bad value 0x{nxt:x}")
            cluster = nxt

    def read_chain(self, first_cluster: int, size: int | None = None) -> bytes:
        payload = bytearray()
        for cluster in self.chain(first_cluster):
            offset = self.cluster_offset(cluster)
            payload.extend(self.image[offset : offset + self.cluster_bytes])
        if size is not None:
            if size > len(payload):
                fail("FAT directory entry size exceeds its cluster chain")
            return bytes(payload[:size])
        return bytes(payload)

    @staticmethod
    def decode_short_name(raw: bytes) -> str:
        stem = raw[:8].decode("ascii", errors="strict").rstrip(" ")
        ext = raw[8:11].decode("ascii", errors="strict").rstrip(" ")
        return f"{stem}.{ext}" if ext else stem

    def directory(self, first_cluster: int) -> dict[str, DirEntry]:
        data = self.read_chain(first_cluster)
        entries: dict[str, DirEntry] = {}
        for offset in range(0, len(data), 32):
            entry = data[offset : offset + 32]
            if len(entry) < 32 or entry[0] == 0x00:
                break
            if entry[0] == 0xE5 or entry[11] == 0x0F:
                continue
            attributes = entry[11]
            if attributes & 0x08:  # volume label
                continue
            name = self.decode_short_name(entry[:11])
            if name in (".", ".."):
                continue
            first = (u16(entry, 20) << 16) | u16(entry, 26)
            size = u32(entry, 28)
            entries[name.upper()] = DirEntry(name, attributes, first, size)
        return entries

    def find(self, path: str) -> DirEntry:
        parts = [part.upper() for part in path.replace("\\", "/").split("/") if part]
        if not parts:
            fail("empty FAT path")
        cluster = self.root_cluster
        for index, part in enumerate(parts):
            entries = self.directory(cluster)
            entry = entries.get(part)
            if entry is None:
                available = ", ".join(sorted(entries)) or "<empty>"
                fail(f"ESP path component {part!r} missing; directory contains: {available}")
            final = index == len(parts) - 1
            if final:
                return entry
            if (entry.attributes & 0x10) == 0 or entry.first_cluster < 2:
                fail(f"ESP path component {part!r} is not a directory")
            cluster = entry.first_cluster
        raise AssertionError("unreachable")

    def read_file(self, path: str) -> bytes:
        entry = self.find(path)
        if entry.attributes & 0x10:
            fail(f"ESP path {path!r} is a directory")
        if entry.first_cluster < 2 and entry.size != 0:
            fail(f"ESP file {path!r} has no data cluster")
        return b"" if entry.size == 0 else self.read_chain(entry.first_cluster, entry.size)


def validate(disk_path: Path, efi_path: Path | None, kernel_path: Path | None) -> None:
    image = disk_path.read_bytes()
    if len(image) < 4 * SECTOR or len(image) % SECTOR != 0:
        fail(f"{disk_path} is not a sector-aligned disk image")
    if image[510:512] != b"\x55\xaa":
        fail("protective MBR signature is missing")

    header = bytearray(image[SECTOR : 2 * SECTOR])
    if header[:8] != b"EFI PART":
        fail("primary GPT signature is missing")
    header_size = u32(header, 12)
    if header_size < 92 or header_size > SECTOR:
        fail(f"invalid GPT header size {header_size}")
    stored_header_crc = u32(header, 16)
    struct.pack_into("<I", header, 16, 0)
    computed_header_crc = binascii.crc32(header[:header_size]) & 0xFFFFFFFF
    if stored_header_crc != computed_header_crc:
        fail("primary GPT header CRC does not match")

    entries_lba = u64(header, 72)
    entry_count = u32(header, 80)
    entry_size = u32(header, 84)
    stored_entries_crc = u32(header, 88)
    if entry_count == 0 or entry_size < 128 or entry_size % 8 != 0:
        fail("GPT partition-entry geometry is invalid")
    entries_bytes = entry_count * entry_size
    entries_offset = entries_lba * SECTOR
    if entries_offset + entries_bytes > len(image):
        fail("GPT partition array extends beyond the disk")
    entries = image[entries_offset : entries_offset + entries_bytes]
    if (binascii.crc32(entries) & 0xFFFFFFFF) != stored_entries_crc:
        fail("GPT partition-array CRC does not match")

    esp: tuple[int, int] | None = None
    for index in range(entry_count):
        entry = entries[index * entry_size : (index + 1) * entry_size]
        if entry[:16] == b"\0" * 16:
            continue
        type_guid = uuid.UUID(bytes_le=entry[:16])
        first_lba = u64(entry, 32)
        last_lba = u64(entry, 40)
        if type_guid == ESP_GUID:
            if esp is not None:
                fail("disk contains more than one EFI System Partition")
            if last_lba < first_lba:
                fail("ESP has an invalid LBA range")
            esp = (first_lba, last_lba - first_lba + 1)
    if esp is None:
        fail("GPT contains no EFI System Partition")

    fat = Fat32(image, esp[0], esp[1])
    efi = fat.read_file("EFI/BOOT/BOOTAA64.EFI")
    kernel = fat.read_file("EFI/AEGIS/AEGISOS.ELF")
    if not efi.startswith(b"MZ"):
        fail("EFI/BOOT/BOOTAA64.EFI is not a PE/COFF image")
    if not kernel.startswith(b"\x7fELF"):
        fail("EFI/AEGIS/AEGISOS.ELF is not an ELF image")

    if efi_path is not None and efi != efi_path.read_bytes():
        fail("ESP BOOTAA64.EFI does not match the freshly built loader")
    if kernel_path is not None and kernel != kernel_path.read_bytes():
        fail("ESP AEGISOS.ELF does not match the freshly built UEFI kernel")

    print(
        "AegisOS PR1 UEFI disk validation passed: "
        f"esp-lba={esp[0]} sectors={esp[1]} "
        f"bootaa64-size={len(efi)} bootaa64-sha256={hashlib.sha256(efi).hexdigest()} "
        f"kernel-size={len(kernel)} kernel-sha256={hashlib.sha256(kernel).hexdigest()}"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path)
    parser.add_argument("--efi", type=Path)
    parser.add_argument("--kernel", type=Path)
    args = parser.parse_args()
    validate(args.image, args.efi, args.kernel)


if __name__ == "__main__":
    main()
