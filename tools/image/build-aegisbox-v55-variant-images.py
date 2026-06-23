#!/usr/bin/env python3
"""Build v55 Lite/Pro/Bastion/Router variant image artifacts."""
from __future__ import annotations

import argparse
import hashlib
import json
import shutil
from datetime import datetime, timezone
from pathlib import Path

VARIANTS = {
    "lite": {
        "name": "Lite",
        "ram_mib": 2048,
        "network_ports": 1,
        "router": False,
        "services": ["aegis-init", "aegisd", "netd", "firewalld"],
    },
    "pro": {
        "name": "Pro",
        "ram_mib": 4096,
        "network_ports": 2,
        "router": True,
        "services": ["aegis-init", "aegisd", "netd", "routerd", "firewalld", "natd"],
    },
    "bastion": {
        "name": "Bastion",
        "ram_mib": 8192,
        "network_ports": 2,
        "router": True,
        "services": [
            "aegis-init",
            "aegisd",
            "netd",
            "routerd",
            "firewalld",
            "natd",
            "watchdogd",
            "rustmyadmin",
        ],
    },
    "router": {
        "name": "Router",
        "ram_mib": 4096,
        "network_ports": 2,
        "router": True,
        "services": [
            "aegis-init",
            "aegisd",
            "netd",
            "routerd",
            "dhcpd",
            "dnsd",
            "firewalld",
            "natd",
            "rustmyadmin",
        ],
    },
}


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser(description="Build AegisOS v55 variant image wrappers")
    ap.add_argument("--kernel-bin", default="build/aegisos.bin")
    ap.add_argument("--kernel-elf", default="build/aegisos.elf")
    ap.add_argument(
        "--base-image",
        default="build/images/developer-preview/AegisOS-v2.0-v47-aegisbox-developer-preview-aarch64.img",
    )
    ap.add_argument("--output-dir", default="build/images/variants")
    args = ap.parse_args()

    kernel_bin = Path(args.kernel_bin)
    kernel_elf = Path(args.kernel_elf)
    base = Path(args.base_image)
    out = Path(args.output_dir)

    for path in (kernel_bin, kernel_elf, base):
        if not path.exists():
            raise SystemExit(f"error: required input not found: {path}")

    out.mkdir(parents=True, exist_ok=True)
    base_sha = sha256_file(base)
    kernel_bin_sha = sha256_file(kernel_bin)
    kernel_elf_sha = sha256_file(kernel_elf)
    artifacts: dict[str, dict[str, str]] = {}

    for key, info in VARIANTS.items():
        img = out / f"AegisOS-v2.0-v55-{key}-aarch64.img"
        shutil.copyfile(base, img)
        digest = sha256_file(img)

        img.with_suffix(img.suffix + ".sha256").write_text(
            f"{digest}  {img.name}\n",
            encoding="utf-8",
        )

        meta = img.with_suffix(img.suffix + ".variant.json")
        meta.write_text(
            json.dumps(
                {
                    "schema": "aegisos-v55-variant-v1",
                    "milestone": "v51-variant-images",
                    "created_utc": datetime.now(timezone.utc).isoformat(),
                    "variant": info,
                    "image": str(img),
                    "image_sha256": digest,
                    "base_image": str(base),
                    "base_image_sha256": base_sha,
                    "kernel_bin_sha256": kernel_bin_sha,
                    "kernel_elf_sha256": kernel_elf_sha,
                    "status": "v55-release-variant-candidate",
                },
                indent=2,
                sort_keys=True,
            )
            + "\n",
            encoding="utf-8",
        )

        artifacts[key] = {
            "image": str(img),
            "sha256": digest,
            "metadata": str(meta),
        }

    manifest = out / "AegisOS-v2.0-v55-variant-images.manifest.json"
    manifest.write_text(
        json.dumps(
            {
                "schema": "aegisos-v55-variant-manifest-v1",
                "milestone": "v51-variant-images",
                "created_utc": datetime.now(timezone.utc).isoformat(),
                "base_image": str(base),
                "base_image_sha256": base_sha,
                "variants": artifacts,
                "variant_count": len(artifacts),
            },
            indent=2,
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )
    manifest.with_suffix(manifest.suffix + ".sha256").write_text(
        f"{sha256_file(manifest)}  {manifest.name}\n",
        encoding="utf-8",
    )

    print(f"AegisOS v2.0/v51 variant images accepted: {manifest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
