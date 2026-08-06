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
EXTRA_LDFLAGS="${EXTRA_LDFLAGS:-}"
LINK_LOAD_BASE="${AEGISOS_LINK_LOAD_BASE:-}"
NATIVE_USER_IMPL="${NATIVE_USER_IMPL:-assembly}"
if [[ "$QEMU_SMOKE" == "1" ]]; then
  EXTRA_CFLAGS="$EXTRA_CFLAGS -DAEGISOS_QEMU_SMOKE=1"
fi
if [[ -n "$LINK_LOAD_BASE" ]]; then
  EXTRA_LDFLAGS="$EXTRA_LDFLAGS --defsym=AEGISOS_LOAD_BASE=$LINK_LOAD_BASE"
fi

command -v clang >/dev/null 2>&1 || { echo "error: clang not found" >&2; exit 127; }
command -v ld.lld >/dev/null 2>&1 || { echo "error: ld.lld not found" >&2; exit 127; }
command -v llvm-objcopy >/dev/null 2>&1 || { echo "error: llvm-objcopy not found" >&2; exit 127; }

# The Makefile object names do not encode EXTRA_CFLAGS, so a QEMU smoke build
# after a normal build can otherwise reuse stale non-smoke objects.
if [[ "${AEGISOS_FORCE_REBUILD:-0}" == "1" ]]; then
  if [[ "$BUILD_DIR" == "build" ]]; then
    # PR1 proof and release evidence lives under build/pr1. A normal forced
    # kernel rebuild must invalidate kernel objects without destroying
    # evidence produced by earlier QEMU/hardware/release gates.
    mkdir -p "$BUILD_DIR"
    find "$BUILD_DIR" -mindepth 1 -maxdepth 1 ! -name pr1 \
      -exec rm -rf -- {} +
  else
    rm -rf "$BUILD_DIR"
  fi
fi

exec make kernel \
  BOARD="$BOARD" \
  BUILD_DIR="$BUILD_DIR" \
  CC='clang --target=aarch64-none-elf' \
  LD=ld.lld \
  OBJCOPY=llvm-objcopy \
  NATIVE_USER_IMPL="$NATIVE_USER_IMPL" \
  EXTRA_CFLAGS="$EXTRA_CFLAGS" \
  EXTRA_LDFLAGS="$EXTRA_LDFLAGS"
