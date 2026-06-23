#!/usr/bin/env python3
"""Finalize the v55 Release IMG manifest and release image artifact."""
from __future__ import annotations

import argparse
import hashlib
import json
import shutil
from datetime import datetime, timezone
from pathlib import Path


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser(description="Finalize AegisOS v55 Release IMG")
    ap.add_argument("--kernel-bin", default="build/aegisos.bin")
    ap.add_argument("--kernel-elf", default="build/aegisos.elf")
    ap.add_argument("--router-image", default="build/images/variants/AegisOS-v2.0-v55-router-aarch64.img")
    ap.add_argument("--output-dir", default="build/images/release-v55")
    args = ap.parse_args()

    kernel_bin = Path(args.kernel_bin)
    kernel_elf = Path(args.kernel_elf)
    router = Path(args.router_image)
    out = Path(args.output_dir)

    for path in (kernel_bin, kernel_elf, router):
        if not path.exists():
            raise SystemExit(f"error: required input not found: {path}")

    out.mkdir(parents=True, exist_ok=True)
    release_img = out / "AegisOS-v2.0-v55-release-aarch64.img"
    shutil.copyfile(router, release_img)
    img_sha = sha256_file(release_img)

    release_img.with_suffix(release_img.suffix + ".sha256").write_text(
        f"{img_sha}  {release_img.name}\n",
        encoding="utf-8",
    )

    manifest = {
        "schema": "aegisos-v55-release-manifest-v1",
        "name": "AegisOS v2.0 Release IMG",
        "milestone": "v55-release-img",
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "arch": "aarch64",
        "release_image": str(release_img),
        "release_image_sha256": img_sha,
        "kernel_bin": str(kernel_bin),
        "kernel_bin_sha256": sha256_file(kernel_bin),
        "kernel_elf": str(kernel_elf),
        "kernel_elf_sha256": sha256_file(kernel_elf),
        "source_router_variant": str(router),
        "source_router_variant_sha256": sha256_file(router),
        "promoted_milestones": [
            "v39 packageable image",
            "v40 flash layout + QEMU disk/image boot + persistent config",
            "v41 block/storage abstraction + process table cleanup",
            "v42 real multiprocess launch + syscall expansion",
            "v43.4 IPC/service messaging",
            "v44 service supervisor + fault/page-fault proof",
            "v45 user/kernel page-table isolation",
            "v46 network bring-up + DHCP + NAT/firewall control plane",
            "v47 AegisBox Developer Preview IMG",
            "v48 release polish",
            "v50 installer/flash flow hardening",
            "v51 Lite/Pro/Bastion/Router image variants",
            "v52 security hardening + service defaults freeze",
            "v53 docs + known limitations + hardware notes",
            "v54 final RC audit/signing/checksums",
            "v55 Release IMG",
        ],
        "release_status": "release-img-candidate",
        "boot_model": "-kernel build/aegisos.elf plus attached raw flash image; standalone firmware bootloader still future work",
        "known_limitations": "See docs/KNOWN_LIMITATIONS.md",
        "hardware_notes": "See docs/HARDWARE_NOTES.md",
    }

    signing = out / "AegisOS-v2.0-v55-release-signing-manifest.json"
    signing.write_text(
        json.dumps(
            {
                "schema": "aegisos-v55-signing-manifest-v1",
                "status": "checksum-signed-placeholder",
                "note": "Cryptographic signing key integration is reserved for production signing infrastructure. SHA-256 sidecars are generated now.",
                "release_image": str(release_img),
                "release_image_sha256": img_sha,
            },
            indent=2,
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )

    manifest_path = out / "AegisOS-v2.0-v55-release-manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    manifest_path.with_suffix(manifest_path.suffix + ".sha256").write_text(
        f"{sha256_file(manifest_path)}  {manifest_path.name}\n",
        encoding="utf-8",
    )
    signing.with_suffix(signing.suffix + ".sha256").write_text(
        f"{sha256_file(signing)}  {signing.name}\n",
        encoding="utf-8",
    )

    print(f"AegisOS v2.0/v55 Release IMG created: {release_img}")
    print(f"AegisOS v2.0/v55 Release manifest: {manifest_path}")
    print(f"AegisOS v2.0/v55 Release sha256: {release_img.with_suffix(release_img.suffix + '.sha256')}")
    print(f"AegisOS v2.0/v55 signing manifest: {signing}")
    print(f"AegisOS v2.0/v55 Release IMG accepted: {release_img}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
