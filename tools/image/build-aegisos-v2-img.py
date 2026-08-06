#!/usr/bin/env python3
"""Build a deterministic AegisOS-v2.0 router raw image container.

This v39 tool intentionally avoids root-only partitioning tools so the image can
be produced on normal developer machines and CI. It writes a raw image with a
small AegisOS header, the built kernel payload, and metadata blocks that reserve
space for later GPT/FAT/ext4 production layout work.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
from datetime import datetime, timezone

MAGIC = b"AEGISOS_V2_IMAGE\n"
DEFAULT_SIZE = 64 * 1024 * 1024
KERNEL_OFFSET = 1 * 1024 * 1024
METADATA_OFFSET = 8 * 1024 * 1024
ROOTFS_OFFSET = 16 * 1024 * 1024


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def write_at(out, offset: int, data: bytes) -> None:
    out.seek(offset)
    out.write(data)


def main() -> int:
    ap = argparse.ArgumentParser(description='Build AegisOS-v2.0 router .img')
    ap.add_argument('--kernel-bin', default='build/aegisos.bin')
    ap.add_argument('--kernel-elf', default='build/aegisos.elf')
    ap.add_argument('--output', default='build/images/AegisOS-v2.0-router-aarch64.img')
    ap.add_argument('--size-mib', type=int, default=64)
    args = ap.parse_args()

    kernel_bin = Path(args.kernel_bin)
    kernel_elf = Path(args.kernel_elf)
    output = Path(args.output)
    size = max(args.size_mib * 1024 * 1024, DEFAULT_SIZE)

    if not kernel_bin.exists():
        raise SystemExit(f'error: kernel binary not found: {kernel_bin}')
    if not kernel_elf.exists():
        raise SystemExit(f'error: kernel ELF not found: {kernel_elf}')

    kernel_bytes = kernel_bin.read_bytes()
    if KERNEL_OFFSET + len(kernel_bytes) >= METADATA_OFFSET:
        raise SystemExit('error: kernel payload overlaps metadata region')

    output.parent.mkdir(parents=True, exist_ok=True)

    manifest = {
        'name': 'AegisOS',
        'release': 'AegisOS-v2.0',
        'milestone': 'v39-router-img',
        'arch': 'aarch64',
        'target': 'AegisBox router / QEMU virt bring-up',
        'created_utc': datetime.now(timezone.utc).isoformat(),
        'image_size_bytes': size,
        'layout': {
            'header_offset': 0,
            'kernel_payload_offset': KERNEL_OFFSET,
            'metadata_offset': METADATA_OFFSET,
            'reserved_rootfs_offset': ROOTFS_OFFSET,
        },
        'payloads': {
            'kernel_bin': str(kernel_bin),
            'kernel_bin_size': len(kernel_bytes),
            'kernel_bin_sha256': sha256_file(kernel_bin),
            'kernel_elf': str(kernel_elf),
            'kernel_elf_sha256': sha256_file(kernel_elf),
        },
        'boot_chain_proven_through': [
            'v27 VFS/initramfs mounts',
            'v28 kernel services',
            'v29 userland handoff',
            'v30 pre-EL0 process descriptor',
            'v31 controlled EL0 transition',
            'v32 first tiny user process',
            'v33 syscall return path',
            'v34 user process exit',
            'v35 ELF validation/bind',
            'v36 file-backed initramfs ELF loading',
            'v37 PT_LOAD copy and permission metadata',
            'v38 argv/envp bootstrap and multiple descriptors',
            'v39 packageable image container',
        ],
        'planned_processes': [
            '/sbin/aegis-init',
            'aegisd',
            'routerd',
            'rustmyadmin',
        ],
        'status': 'stage-one packageable raw image container; not final production flashing image',
    }

    header = {
        'magic': MAGIC.decode().strip(),
        'format_version': 1,
        'release': manifest['release'],
        'milestone': manifest['milestone'],
        'kernel_offset': KERNEL_OFFSET,
        'metadata_offset': METADATA_OFFSET,
        'rootfs_offset': ROOTFS_OFFSET,
    }
    header_bytes = (MAGIC + json.dumps(header, sort_keys=True).encode('utf-8') + b'\n')
    metadata_bytes = json.dumps(manifest, indent=2, sort_keys=True).encode('utf-8') + b'\n'

    if len(header_bytes) > 4096:
        raise SystemExit('error: header exceeds reserved 4KiB')
    if len(metadata_bytes) > (ROOTFS_OFFSET - METADATA_OFFSET):
        raise SystemExit('error: metadata block exceeds reserved range')

    with output.open('wb') as out:
        out.truncate(size)
        write_at(out, 0, header_bytes)
        write_at(out, KERNEL_OFFSET, kernel_bytes)
        write_at(out, METADATA_OFFSET, metadata_bytes)
        # Human-readable rootfs placeholder for future real filesystems.
        rootfs_note = (
            'AEGISOS_V2_ROOTFS_RESERVED\n'
            'This v39 region is reserved for the future router rootfs/config partition.\n'
            'The current image packages the proven kernel/userland bring-up payload and metadata.\n'
        ).encode('utf-8')
        write_at(out, ROOTFS_OFFSET, rootfs_note)

    image_sha = sha256_file(output)
    manifest['image'] = str(output)
    manifest['image_sha256'] = image_sha
    manifest_path = output.with_suffix(output.suffix + '.manifest.json')
    sha_path = output.with_suffix(output.suffix + '.sha256')
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
    sha_path.write_text(f'{image_sha}  {output.name}\n')

    print(f'AegisOS v2.0 image created: {output}')
    print(f'AegisOS v2.0 manifest: {manifest_path}')
    print(f'AegisOS v2.0 sha256: {sha_path}')
    print(f'AegisOS v2.0/v39 image packaging accepted: {output}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
