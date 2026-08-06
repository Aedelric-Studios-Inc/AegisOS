#!/usr/bin/env bash
set -euo pipefail
if ! command -v qemu-system-aarch64 >/dev/null 2>&1; then
  echo "qemu-system-aarch64: missing"
  exit 127
fi
qemu-system-aarch64 --version
printf '\n--- machine help grep virt ---\n'
qemu-system-aarch64 -machine help | grep -E 'virt|none' || true
printf '\n--- cpu help grep cortex/aarch64 ---\n'
qemu-system-aarch64 -cpu help | grep -E 'cortex|aarch64|max' | head -50 || true
