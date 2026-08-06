#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

OUT="build/images/pr1/AegisOS-2.0.0-pre.1-aarch64-uefi.img"
SIZE="${AEGISOS_PR1_DISK_MIB:-256}"
SKIP_BUILD="${AEGISOS_SKIP_BUILD:-0}"
UEFI_KERNEL_BUILD_DIR="${AEGISOS_UEFI_KERNEL_BUILD_DIR:-build/uefi-kernel}"
UEFI_LOAD_BASE="${AEGISOS_UEFI_LOAD_BASE:-0x50080000}"
KERNEL="${AEGISOS_PR1_KERNEL:-$UEFI_KERNEL_BUILD_DIR/aegisos.elf}"
EFI="${AEGISOS_PR1_EFI:-build/uefi/BOOTAA64.EFI}"
BOARD="${BOARD:-bastion}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --out) OUT="$2"; shift 2 ;;
    --size-mib) SIZE="$2"; shift 2 ;;
    --kernel) KERNEL="$2"; shift 2 ;;
    --efi) EFI="$2"; shift 2 ;;
    --board) BOARD="$2"; shift 2 ;;
    --load-base) UEFI_LOAD_BASE="$2"; shift 2 ;;
    --skip-build) SKIP_BUILD=1; shift ;;
    *) echo "error: unknown argument: $1" >&2; exit 2 ;;
  esac
done

if [[ "$SKIP_BUILD" != 1 ]]; then
  BOARD="$BOARD" \
  BUILD_DIR="$UEFI_KERNEL_BUILD_DIR" \
  AEGISOS_FORCE_REBUILD=1 \
  AEGISOS_LINK_LOAD_BASE="$UEFI_LOAD_BASE" \
    tools/build/kernel-clang-aarch64.sh
  KERNEL="$UEFI_KERNEL_BUILD_DIR/aegisos.elf"
  tools/boot/build-aegisos-uefi-loader.sh "$EFI"
fi

[[ -f "$KERNEL" ]] || { echo "error: UEFI kernel missing: $KERNEL" >&2; exit 2; }
[[ -f "$EFI" ]] || { echo "error: UEFI loader missing: $EFI" >&2; exit 2; }

python3 tools/image/build-pr1-uefi-disk.py \
  --efi "$EFI" \
  --kernel "$KERNEL" \
  --out "$OUT" \
  --size-mib "$SIZE"
python3 tools/image/validate-pr1-uefi-disk.py \
  "$OUT" \
  --efi "$EFI" \
  --kernel "$KERNEL"
sha256sum "$OUT" > "$OUT.sha256"
printf 'AegisOS PR1 UEFI kernel: %s (load base %s)\n' "$KERNEL" "$UEFI_LOAD_BASE"
printf 'AegisOS PR1 UEFI disk: %s\n' "$OUT"
