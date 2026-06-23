#!/usr/bin/env python3
"""Build the v40 AegisOS-v2.0 flash-style router image.

v39 produced a packageable raw image container. v40 turns that into a
flash-layout image with an MBR partition table and dedicated boot/root/config
regions that can be attached to QEMU as a block device while the proven kernel
bring-up path is still launched with -kernel.

This intentionally avoids root-only mkfs/losetup/parted dependencies so it can
run on a normal developer machine and in CI. The partition regions are real,
offset-stable raw partitions with AegisOS headers and metadata. FAT/ext4
formatting is the next production-flash refinement, not required for the v40
layout proof.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
from datetime import datetime, timezone
from pathlib import Path

SECTOR = 512
MIB = 1024 * 1024
MAGIC = b"AEGISOS_V2_FLASH\n"
DEFAULT_SIZE_MIB = 256

BOOT_START_LBA = 2048                         # 1 MiB alignment
BOOT_SIZE_MIB = 32
ROOT_SIZE_MIB = 160
CONFIG_SIZE_MIB = 32

BOOT_TYPE = 0x0C                              # FAT32 LBA-style boot region placeholder
ROOT_TYPE = 0x83                              # Linux/root region placeholder
CONFIG_TYPE = 0xDA                            # Non-FS data/config region


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sectors(mib: int) -> int:
    return (mib * MIB) // SECTOR


def lba_to_offset(lba: int) -> int:
    return lba * SECTOR


def mbr_partition_entry(status: int, ptype: int, start_lba: int, sector_count: int) -> bytes:
    # CHS values are deliberately set to max/placeholder; QEMU/Linux tools rely on LBA.
    start_chs = b"\x00\x02\x00"
    end_chs = b"\xff\xff\xff"
    return struct.pack("<B3sB3sII", status, start_chs, ptype, end_chs, start_lba, sector_count)


def build_mbr(parts: list[dict]) -> bytes:
    mbr = bytearray(SECTOR)
    mbr[0:16] = b"AEGISOSV2FLASH\0"
    table = bytearray()
    for part in parts[:4]:
        table += mbr_partition_entry(part["status"], part["type"], part["start_lba"], part["sectors"])
    while len(table) < 16 * 4:
        table += b"\x00" * 16
    mbr[446:446 + 64] = table[:64]
    mbr[510:512] = b"\x55\xaa"
    return bytes(mbr)


def write_at(out, offset: int, data: bytes) -> None:
    out.seek(offset)
    out.write(data)


def partition_header(name: str, role: str, payload: dict) -> bytes:
    body = {
        "name": name,
        "role": role,
        "format": "aegisos-raw-region-v1",
        "payload": payload,
    }
    return (f"AEGISOS_PARTITION:{name}\n" + json.dumps(body, indent=2, sort_keys=True) + "\n").encode("utf-8")


def main() -> int:
    ap = argparse.ArgumentParser(description="Build AegisOS-v2.0 v40 flash-layout image")
    ap.add_argument("--kernel-bin", default="build/aegisos.bin")
    ap.add_argument("--kernel-elf", default="build/aegisos.elf")
    ap.add_argument("--output", default="build/images/AegisOS-v2.0-router-aarch64-v40-flash.img")
    ap.add_argument("--size-mib", type=int, default=DEFAULT_SIZE_MIB)
    args = ap.parse_args()

    kernel_bin = Path(args.kernel_bin)
    kernel_elf = Path(args.kernel_elf)
    output = Path(args.output)
    if not kernel_bin.exists():
        raise SystemExit(f"error: kernel binary not found: {kernel_bin}")
    if not kernel_elf.exists():
        raise SystemExit(f"error: kernel ELF not found: {kernel_elf}")

    size_mib = max(args.size_mib, DEFAULT_SIZE_MIB)
    total_bytes = size_mib * MIB
    total_sectors = total_bytes // SECTOR

    boot_start = BOOT_START_LBA
    boot_count = sectors(BOOT_SIZE_MIB)
    root_start = boot_start + boot_count
    root_count = sectors(ROOT_SIZE_MIB)
    config_start = root_start + root_count
    config_count = sectors(CONFIG_SIZE_MIB)

    if config_start + config_count >= total_sectors:
        raise SystemExit("error: image is too small for v40 boot/root/config layout")

    parts = [
        {"index": 1, "name": "AEGIS_BOOT", "role": "boot", "status": 0x80, "type": BOOT_TYPE, "start_lba": boot_start, "sectors": boot_count},
        {"index": 2, "name": "AEGIS_ROOT", "role": "root", "status": 0x00, "type": ROOT_TYPE, "start_lba": root_start, "sectors": root_count},
        {"index": 3, "name": "AEGIS_CONFIG", "role": "persistent-config", "status": 0x00, "type": CONFIG_TYPE, "start_lba": config_start, "sectors": config_count},
    ]
    for part in parts:
        part["offset"] = lba_to_offset(part["start_lba"])
        part["size_bytes"] = part["sectors"] * SECTOR

    kernel_bytes = kernel_bin.read_bytes()
    boot_kernel_offset = parts[0]["offset"] + MIB
    if boot_kernel_offset + len(kernel_bytes) > parts[0]["offset"] + parts[0]["size_bytes"]:
        raise SystemExit("error: kernel does not fit in v40 boot partition region")

    default_config = {
        "schema": "aegisos-config-v1",
        "release": "AegisOS-v2.0",
        "milestone": "v40-flash-layout-disk-config",
        "active_profile": "aegisbox-bastion",
        "profiles": {
            "aegisbox-lite": {"services": ["aegis-init", "aegisd", "firewall-lite"], "mode": "lite-security-appliance"},
            "aegisbox-pro": {"services": ["aegis-init", "aegisd", "routerd", "backupd"], "mode": "pro-security-appliance"},
            "aegisbox-bastion": {"services": ["aegis-init", "aegisd", "routerd", "rustmyadmin"], "mode": "bastion-hardened"},
            "aedelric-router": {"services": ["aegis-init", "aegisd", "routerd", "dhcpd", "dnsd", "firewalld", "rustmyadmin"], "mode": "router"},
        },
        "network": {
            "wan": "eth0",
            "lan": "eth1",
            "dhcp": {"enabled": False, "planned_range": "192.168.50.100-192.168.50.240"},
            "nat": {"enabled": False},
            "firewall": {"default_policy": "deny-inbound"},
        },
        "admin": {
            "rustmyadmin_handoff": "planned-v47",
            "console": "serial-qemu",
        },
        "persistence": {
            "partition": "AEGIS_CONFIG",
            "writable_after_mount": True,
            "format": "aegisos-config-json-v1",
        },
    }
    config_payload = json.dumps(default_config, indent=2, sort_keys=True).encode("utf-8") + b"\n"
    config_header = partition_header("AEGIS_CONFIG", "persistent-config", {
        "config_sha256": sha256_bytes(config_payload),
        "config_size": len(config_payload),
    })

    manifest = {
        "name": "AegisOS",
        "release": "AegisOS-v2.0",
        "milestone": "v40-flash-layout-disk-config",
        "arch": "aarch64",
        "target": "AegisBox / Aedelric Router flash-layout bring-up",
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "image_size_bytes": total_bytes,
        "sector_size": SECTOR,
        "partition_table": "MBR",
        "partitions": parts,
        "boot_mode": {
            "current_qemu_path": "-kernel build/aegisos.bin with v40 flash image attached as virtio block device",
            "future_production_path": "firmware/bootloader loads kernel from AEGIS_BOOT partition",
        },
        "payloads": {
            "kernel_bin": str(kernel_bin),
            "kernel_bin_size": len(kernel_bytes),
            "kernel_bin_sha256": sha256_file(kernel_bin),
            "kernel_elf": str(kernel_elf),
            "kernel_elf_sha256": sha256_file(kernel_elf),
            "boot_kernel_offset": boot_kernel_offset,
            "persistent_config_sha256": sha256_bytes(config_payload),
        },
        "boot_chain_proven_through": [
            "v35 ELF validation/bind",
            "v36 VFS/initramfs file-backed ELF loading",
            "v37 PT_LOAD copy and permission metadata",
            "v38 argv/envp bootstrap and multiple descriptors",
            "v39 packageable router image",
            "v40 flash layout, QEMU image attachment path, persistent config partition",
        ],
        "status": "v40 flash-style layout image; QEMU disk attachment supported; full firmware disk boot remains a later bootloader/partition-filesystem milestone",
    }
    manifest_bytes = json.dumps(manifest, indent=2, sort_keys=True).encode("utf-8") + b"\n"

    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("wb") as out:
        out.truncate(total_bytes)
        write_at(out, 0, build_mbr(parts))
        write_at(out, SECTOR, MAGIC + json.dumps({
            "release": "AegisOS-v2.0",
            "milestone": "v40-flash-layout-disk-config",
            "partition_table": "MBR",
            "image_size_bytes": total_bytes,
        }, sort_keys=True).encode("utf-8") + b"\n")
        write_at(out, parts[0]["offset"], partition_header("AEGIS_BOOT", "boot", {
            "kernel_offset": boot_kernel_offset,
            "kernel_size": len(kernel_bytes),
            "kernel_sha256": sha256_file(kernel_bin),
        }))
        write_at(out, boot_kernel_offset, kernel_bytes)
        write_at(out, parts[1]["offset"], partition_header("AEGIS_ROOT", "root", {
            "state": "reserved-for-userland-rootfs",
            "init": "/sbin/aegis-init currently supplied by builtin initramfs proof path",
        }))
        write_at(out, parts[1]["offset"] + MIB, manifest_bytes)
        write_at(out, parts[2]["offset"], config_header)
        write_at(out, parts[2]["offset"] + 64 * 1024, config_payload)

    image_sha = sha256_file(output)
    manifest["image"] = str(output)
    manifest["image_sha256"] = image_sha
    manifest_path = output.with_suffix(output.suffix + ".manifest.json")
    sha_path = output.with_suffix(output.suffix + ".sha256")
    config_path = output.with_suffix(output.suffix + ".config.json")
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    sha_path.write_text(f"{image_sha}  {output.name}\n")
    config_path.write_bytes(config_payload)

    print(f"AegisOS v2.0/v40 flash image created: {output}")
    print(f"AegisOS v2.0/v40 manifest: {manifest_path}")
    print(f"AegisOS v2.0/v40 sha256: {sha_path}")
    print(f"AegisOS v2.0/v40 persistent config: {config_path}")
    print(f"AegisOS v2.0/v40 flash image layout accepted: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
