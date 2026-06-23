#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
need_file() { [[ -f "$1" ]] || { echo "error: v40 guard failed: missing $1" >&2; exit 2; }; }
need_grep() { grep -q "$1" "$2" || { echo "error: v40 guard failed: missing '$1' in $2" >&2; exit 2; }; }
if [[ ! -f VERSION ]] || ! grep -Eq 'AegisOS-v2\.0-v40-flash-layout-disk-config' VERSION; then
  echo "error: v40 guard failed: stale or wrong VERSION" >&2
  exit 2
fi
need_file README.md
need_file docs/FLASH_IMAGE_LAYOUT.md
need_file docs/QEMU_DISK_BOOT.md
need_file docs/PERSISTENT_CONFIG.md
need_file docs/V40_RELEASE_NOTES.md
need_file tools/image/build-aegisos-v2-flash-img.py
need_file tools/image/build-aegisos-v2-flash-img.sh
need_file tools/image/inspect-aegisos-v2-flash-img.py
need_file tools/qemu/boot-v40-disk-image-aarch64.sh
need_file tools/qemu/prove-v40-disk-image-aarch64.sh
need_grep 'AegisOS-v2.0-v40-flash-layout-disk-config' README.md
need_grep 'AEGIS_BOOT' docs/FLASH_IMAGE_LAYOUT.md
need_grep 'QEMU v40 disk/image boot path accepted' tools/qemu/prove-v40-disk-image-aarch64.sh
need_grep 'AEGIS_CONFIG' tools/image/build-aegisos-v2-flash-img.py
need_grep 'assert-v40-milestone.sh' tools/qemu/build-and-smoke-aarch64.sh
need_grep 'build-aegisos-v2-flash-img.sh' tools/qemu/build-and-smoke-aarch64.sh
need_grep 'prove-v40-disk-image-aarch64.sh' tools/qemu/build-and-smoke-aarch64.sh
if compgen -G 'README-v*.md' >/dev/null; then
  echo "error: v40 guard failed: root milestone README-v*.md files should remain consolidated" >&2
  exit 2
fi
echo "v40/AegisOS-v2.0 flash-layout guard: OK (AegisOS-v2.0-v40-flash-layout-disk-config)"
