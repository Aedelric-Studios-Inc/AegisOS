#!/usr/bin/env bash
# Boot the AegisBox Developer Preview IMG in a QEMU AArch64 VM.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

KERNEL="${AEGISOS_QEMU_KERNEL:-build/aegisos.elf}"
IMG="${AEGISOS_QEMU_IMG:-build/images/developer-preview/AegisOS-v2.0-v47-aegisbox-developer-preview-aarch64.img}"
MEMORY="${AEGISOS_QEMU_MEMORY:-1G}"
CPU="${AEGISOS_QEMU_CPU:-cortex-a57}"

if [[ ! -f "$KERNEL" ]]; then
  echo "error: kernel not found: $KERNEL" >&2
  echo "hint: run tools/qemu/build-and-smoke-aarch64.sh first" >&2
  exit 2
fi

if [[ ! -f "$IMG" ]]; then
  echo "error: Developer Preview IMG not found: $IMG" >&2
  echo "hint: run tools/qemu/build-and-smoke-aarch64.sh first" >&2
  exit 2
fi

exec qemu-system-aarch64 \
  -machine virt,secure=off,virtualization=off,gic-version=2 \
  -cpu "$CPU" \
  -smp 1 \
  -m "$MEMORY" \
  -nographic \
  -serial mon:stdio \
  -semihosting-config enable=on,target=native \
  -kernel "$KERNEL" \
  -drive file="$IMG",format=raw,if=none,id=aegisflash \
  -device virtio-blk-device,drive=aegisflash \
  -netdev user,id=net0 \
  -device virtio-net-device,netdev=net0 \
  -append "console=ttyAMA0,115200"
