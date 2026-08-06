#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 1 ]]; then
  echo "usage: $0 /dev/sdX-or-image-file" >&2
  exit 2
fi
TARGET="$1"
IMG="build/images/developer-preview/AegisOS-v2.0-v47-aegisbox-developer-preview-aarch64.img"
if [[ ! -f "$IMG" ]]; then
  echo "error: Developer Preview image not built: $IMG" >&2
  echo "run: tools/qemu/build-and-smoke-aarch64.sh" >&2
  exit 2
fi
cat >&2 <<MSG
AegisOS v47 Developer Preview flash helper
source: $IMG
target: $TARGET

This helper is intentionally guarded for Developer Preview safety.
Set AEGISOS_FLASH_CONFIRM=YES to write with dd.
MSG
if [[ "${AEGISOS_FLASH_CONFIRM:-}" != "YES" ]]; then
  echo "refusing to flash without AEGISOS_FLASH_CONFIRM=YES" >&2
  exit 3
fi
sudo dd if="$IMG" of="$TARGET" bs=4M conv=fsync status=progress
sync
echo "AegisOS v47 Developer Preview image written to $TARGET"
