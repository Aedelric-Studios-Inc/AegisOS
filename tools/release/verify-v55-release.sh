#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
required=(
  "build/aegisos.bin" "build/aegisos.elf"
  "build/images/developer-preview/AegisOS-v2.0-v47-aegisbox-developer-preview-aarch64.img"
  "build/images/variants/AegisOS-v2.0-v55-lite-aarch64.img"
  "build/images/variants/AegisOS-v2.0-v55-pro-aarch64.img"
  "build/images/variants/AegisOS-v2.0-v55-bastion-aarch64.img"
  "build/images/variants/AegisOS-v2.0-v55-router-aarch64.img"
  "build/images/variants/AegisOS-v2.0-v55-variant-images.manifest.json"
  "build/images/release-v55/AegisOS-v2.0-v55-release-aarch64.img"
  "build/images/release-v55/AegisOS-v2.0-v55-release-aarch64.img.sha256"
  "build/images/release-v55/AegisOS-v2.0-v55-release-manifest.json"
  "build/images/release-v55/AegisOS-v2.0-v55-release-manifest.json.sha256"
  "build/images/release-v55/AegisOS-v2.0-v55-release-signing-manifest.json"
  "docs/KNOWN_LIMITATIONS.md" "docs/HARDWARE_NOTES.md" "docs/FLASHING_RELEASE_IMG.md" "docs/V50_V55_RELEASE_NOTES.md"
)
for path in "${required[@]}"; do [[ -f "$path" ]] || { echo "error: missing v55 release artifact: $path" >&2; exit 2; }; done
(cd build/images/variants && sha256sum -c AegisOS-v2.0-v55-lite-aarch64.img.sha256 && sha256sum -c AegisOS-v2.0-v55-pro-aarch64.img.sha256 && sha256sum -c AegisOS-v2.0-v55-bastion-aarch64.img.sha256 && sha256sum -c AegisOS-v2.0-v55-router-aarch64.img.sha256 && sha256sum -c AegisOS-v2.0-v55-variant-images.manifest.json.sha256)
(cd build/images/release-v55 && sha256sum -c AegisOS-v2.0-v55-release-aarch64.img.sha256 && sha256sum -c AegisOS-v2.0-v55-release-manifest.json.sha256 && sha256sum -c AegisOS-v2.0-v55-release-signing-manifest.json.sha256)
python3 - <<'PYVERIFY'
import json
from pathlib import Path
m=json.loads(Path('build/images/release-v55/AegisOS-v2.0-v55-release-manifest.json').read_text())
if m.get('milestone') != 'v55-release-img': raise SystemExit('error: v55 manifest milestone mismatch')
if 'standalone firmware bootloader still future work' not in m.get('boot_model',''): raise SystemExit('error: boot model limitation missing from release manifest')
print('AegisOS v55 release artifact verification accepted.')
PYVERIFY
