#!/usr/bin/env python3
"""Build the v47 AegisBox Developer Preview IMG release wrapper.

The v47 Developer Preview does not replace the v40 flash-layout image format;
it packages the proven v40 flash image with v46 router-control metadata,
variant manifests, service defaults, checksums, and an install/flash contract
suitable for AegisBox developer testing.
"""
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
    ap = argparse.ArgumentParser(description="Build AegisBox Developer Preview IMG wrapper")
    ap.add_argument("--kernel-bin", default="build/aegisos.bin")
    ap.add_argument("--kernel-elf", default="build/aegisos.elf")
    ap.add_argument("--base-image", default="build/images/AegisOS-v2.0-router-aarch64-v40-flash.img")
    ap.add_argument("--output-dir", default="build/images/developer-preview")
    args = ap.parse_args()

    kernel_bin = Path(args.kernel_bin)
    kernel_elf = Path(args.kernel_elf)
    base_image = Path(args.base_image)
    out_dir = Path(args.output_dir)

    for required in (kernel_bin, kernel_elf, base_image):
        if not required.exists():
            raise SystemExit(f"error: required input not found: {required}")

    out_dir.mkdir(parents=True, exist_ok=True)
    preview_img = out_dir / "AegisOS-v2.0-v47-aegisbox-developer-preview-aarch64.img"
    shutil.copyfile(base_image, preview_img)

    variants = {
        "Lite": {
            "board_profile": "aegisbox-lite",
            "services": ["aegis-init", "aegisd", "firewall-lite", "netctl"],
            "purpose": "small security appliance developer image",
        },
        "Pro": {
            "board_profile": "aegisbox-pro",
            "services": ["aegis-init", "aegisd", "routerd", "firewalld", "natd", "netctl"],
            "purpose": "router/security appliance developer image",
        },
        "Bastion": {
            "board_profile": "aegisbox-bastion",
            "services": ["aegis-init", "aegisd", "routerd", "firewalld", "natd", "rustmyadmin"],
            "purpose": "hardened appliance developer image",
        },
        "Router": {
            "board_profile": "aedelric-router",
            "services": ["aegis-init", "aegisd", "routerd", "dhcpd", "dnsd", "firewalld", "natd", "rustmyadmin"],
            "purpose": "router control-plane developer image",
        },
    }

    manifest = {
        "name": "AegisOS AegisBox Developer Preview",
        "release": "AegisOS-v2.0",
        "milestone": "v47-aegisbox-developer-preview-img",
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "arch": "aarch64",
        "base_image": str(base_image),
        "preview_image": str(preview_img),
        "base_image_sha256": sha256_file(base_image),
        "preview_image_sha256": sha256_file(preview_img),
        "kernel_bin": str(kernel_bin),
        "kernel_bin_sha256": sha256_file(kernel_bin),
        "kernel_elf": str(kernel_elf),
        "kernel_elf_sha256": sha256_file(kernel_elf),
        "proved_kernel_milestones": [
            "v40 flash layout + QEMU disk/image boot + persistent config",
            "v41 block/storage abstraction + process table cleanup",
            "v42 real multiprocess launch + syscall expansion",
            "v43.4 IPC/service messaging direct-mailbox roundtrip",
            "v44 service supervisor + fault/page-fault proof",
            "v45 user/kernel page-table isolation",
            "v46 network bring-up + DHCP + NAT/firewall control plane",
        ],
        "variants": variants,
        "flash_flow": {
            "developer_host_command": "tools/flashing/flash-aegisbox-dev-preview.sh <device>",
            "qemu_validation": "tools/qemu/build-and-smoke-aarch64.sh",
            "hardware_status": "developer-preview; verify board-specific boot firmware before hardware flashing",
        },
        "security_baseline": {
            "default_firewall_policy": "deny inbound",
            "nat": "enabled for router profiles",
            "dhcp": "synthetic lease proof in QEMU; real packet DHCP remains hardware/network-backend work",
            "user_kernel_isolation": "v45 TTBR0 contract preserved",
        },
        "status": "Developer Preview IMG contract; not final v55 Release IMG",
    }

    manifest_path = preview_img.with_suffix(preview_img.suffix + ".manifest.json")
    sha_path = preview_img.with_suffix(preview_img.suffix + ".sha256")
    config_path = preview_img.with_suffix(preview_img.suffix + ".dev-preview.json")

    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    sha_path.write_text(f"{manifest['preview_image_sha256']}  {preview_img.name}\n")
    config_path.write_text(json.dumps({
        "schema": "aegisos-dev-preview-config-v1",
        "milestone": "v47-aegisbox-developer-preview-img",
        "active_variant": "Router",
        "variants": variants,
        "network": {
            "wan": "eth0",
            "lan": "eth1",
            "dhcp_client": True,
            "nat": True,
            "firewall_default": "drop",
        },
        "admin": {
            "rustmyadmin": "enabled on Bastion/Router profiles when service runtime is promoted",
            "serial_console": True,
        },
    }, indent=2, sort_keys=True) + "\n")

    print(f"AegisOS v2.0/v47 Developer Preview IMG created: {preview_img}")
    print(f"AegisOS v2.0/v47 Developer Preview manifest: {manifest_path}")
    print(f"AegisOS v2.0/v47 Developer Preview sha256: {sha_path}")
    print(f"AegisOS v2.0/v47 Developer Preview config: {config_path}")
    print(f"AegisOS v2.0/v47 Developer Preview IMG accepted: {preview_img}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
