#!/usr/bin/env bash
# Build the AegisOS AArch64 kernel with Clang/LLD.
# Useful on hosts that do not have aarch64-linux-gnu-gcc installed.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

BOARD="${BOARD:-bastion}"
BUILD_DIR="${BUILD_DIR:-build}"
QEMU_SMOKE="${AEGISOS_QEMU_SMOKE:-0}"
EXTRA_CFLAGS="${EXTRA_CFLAGS:-}"
if [[ "$QEMU_SMOKE" == "1" ]]; then
  EXTRA_CFLAGS="$EXTRA_CFLAGS -DAEGISOS_QEMU_SMOKE=1"
fi

command -v clang >/dev/null 2>&1 || { echo "error: clang not found" >&2; exit 127; }
command -v ld.lld >/dev/null 2>&1 || { echo "error: ld.lld not found" >&2; exit 127; }
command -v llvm-objcopy >/dev/null 2>&1 || { echo "error: llvm-objcopy not found" >&2; exit 127; }

# The Makefile object names do not encode EXTRA_CFLAGS, so a QEMU smoke build
# after a normal build can otherwise reuse stale non-smoke objects.
if [[ "${AEGISOS_FORCE_REBUILD:-0}" == "1" ]]; then
  rm -rf "$BUILD_DIR"
fi

exec make kernel \
  BOARD="$BOARD" \
  CC='clang --target=aarch64-none-elf' \
  LD=ld.lld \
  OBJCOPY=llvm-objcopy \
  EXTRA_CFLAGS="$EXTRA_CFLAGS"
