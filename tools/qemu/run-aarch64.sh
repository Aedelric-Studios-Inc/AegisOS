#!/usr/bin/env bash
# Run AegisOS in QEMU (AArch64 virt machine).
set -euo pipefail

KERNEL="${1:-build/aegisos.bin}"
DTB="${2:-hal/boards/bastion/device_tree.dtb}"

qemu-system-aarch64 \
    -machine virt \
    -cpu cortex-a57 \
    -m 1G \
    -nographic \
    -serial mon:stdio \
    -kernel "$KERNEL" \
    -dtb "$DTB" \
    -append "console=ttyAMA0,115200"
