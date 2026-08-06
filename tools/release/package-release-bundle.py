#!/usr/bin/env python3
"""Generate an AegisOS release-candidate manifest.

This script does not declare a final v55 release. It records and verifies the
current v48/v49 release-candidate artifact set so the release host has a stable
promotion boundary.
"""
from __future__ import annotations

import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build/images/release-candidate"

ARTIFACTS = [
    "build/aegisos.bin",
    "build/aegisos.elf",
    "build/images/AegisOS-v2.0-router-aarch64-v40-flash.img",
    "build/images/AegisOS-v2.0-router-aarch64-v40-flash.img.manifest.json",
    "build/images/AegisOS-v2.0-router-aarch64-v40-flash.img.config.json",
    "build/images/developer-preview/AegisOS-v2.0-v47-aegisbox-developer-preview-aarch64.img",
    "build/images/developer-preview/AegisOS-v2.0-v47-aegisbox-developer-preview-aarch64.img.manifest.json",
    "build/images/developer-preview/AegisOS-v2.0-v47-aegisbox-developer-preview-aarch64.img.dev-preview.json",
    "build/images/release-polish/AegisOS-v2.0-v48-release-polish-manifest.json",
    "build/images/release-polish/AegisOS-v2.0-v48-board-profiles.json",
    "build/images/release-polish/AegisOS-v2.0-v48-service-defaults.json",
]

DOCS = [
    "README.md",
    "RELEASE_CANDIDATE.md",
    "docs/RELEASE_CHECKLIST.md",
    "docs/RELEASE_NOTES_TEMPLATE.md",
    "docs/V48_RELEASE_NOTES.md",
    "docs/V49_RELEASE_CANDIDATE_PREP.md",
    "tools/release/prepare-release-candidate.sh",
    "tools/release/verify-release-artifacts.sh",
    "tools/release/package-release-bundle.py",
    "tools/qemu/boot-dev-preview-vm-aarch64.sh",
]


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def file_entry(rel: str) -> dict:
    path = ROOT / rel
    if not path.exists():
        raise SystemExit(f"error: required release artifact missing: {rel}")
    return {
        "path": rel,
        "size_bytes": path.stat().st_size,
        "sha256": sha256_file(path),
    }


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)

    manifest = {
        "name": "AegisOS v2.0 Release Candidate Manifest",
        "release": "AegisOS-v2.0",
        "milestone": "v49-release-candidate-prep",
        "status": "release-candidate-prep; not final v55 Release IMG",
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "arch": "aarch64",
        "required_proof_chain": [
            "v40 flash layout + QEMU disk/image boot",
            "v41 block/storage abstraction + process table cleanup",
            "v42 real multiprocess launch + syscall expansion",
            "v43.4 IPC/service messaging mailbox roundtrip",
            "v44 service supervisor + fault proof",
            "v45 user/kernel page-table isolation",
            "v46 network bring-up + DHCP + NAT/firewall control plane",
            "v47 AegisBox Developer Preview IMG contract",
            "v48 release-polish gates",
        ],
        "artifacts": [file_entry(rel) for rel in ARTIFACTS],
        "documentation": [file_entry(rel) for rel in DOCS],
        "manual_vm_boot": "tools/qemu/boot-dev-preview-vm-aarch64.sh",
        "formal_proof_command": "tools/qemu/build-and-smoke-aarch64.sh",
        "release_notes_template": "docs/RELEASE_NOTES_TEMPLATE.md",
        "promotion_rule": "Promote to v55 only after release-candidate validation, hardware/VM notes, final checksums/signing, and known limitations are frozen.",
    }

    manifest_path = OUT / "AegisOS-v2.0-release-candidate-manifest.json"
    sha_path = OUT / "AegisOS-v2.0-release-candidate-manifest.sha256"
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    sha_path.write_text(f"{sha256_file(manifest_path)}  {manifest_path.name}\n")

    print(f"AegisOS v2.0 release-candidate manifest created: {manifest_path}")
    print(f"AegisOS v2.0 release-candidate manifest sha256: {sha_path}")
    print(f"AegisOS v2.0 v49 release-candidate prep accepted: {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
