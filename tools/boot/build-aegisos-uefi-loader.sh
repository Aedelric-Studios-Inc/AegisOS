#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
OUT="${1:-build/uefi/BOOTAA64.EFI}"
OBJ_DIR="${AEGISOS_UEFI_OBJ_DIR:-build/uefi}"
mkdir -p "$(dirname "$OUT")" "$OBJ_DIR"

CFLAGS=(
  --target=aarch64-pc-windows-msvc
  -ffreestanding
  -fshort-wchar
  -fno-stack-protector
  -fno-builtin
  -fno-PIE
  -Os
  -Wall
  -Wextra
  -Iboot/uefi
)

clang "${CFLAGS[@]}" -c boot/uefi/aegis_uefi.c -o "$OBJ_DIR/aegis_uefi.obj"
clang --target=aarch64-pc-windows-msvc -ffreestanding -fno-stack-protector -c \
  boot/uefi/handoff.S -o "$OBJ_DIR/handoff.obj"

lld-link \
  /subsystem:efi_application \
  /entry:efi_main \
  /nodefaultlib \
  /machine:arm64 \
  /timestamp:0 \
  /dynamicbase \
  /nxcompat \
  /base:0x70000000 \
  /out:"$OUT" \
  "$OBJ_DIR/aegis_uefi.obj" \
  "$OBJ_DIR/handoff.obj"

python3 tools/boot/validate-uefi-pe.py "$OUT"
printf 'AegisOS UEFI loader: %s\n' "$OUT"
