#!/usr/bin/env bash
# Run AegisOS in QEMU (AArch64 virt machine).
set -euo pipefail

if ! command -v qemu-system-aarch64 >/dev/null 2>&1; then
    echo "error: qemu-system-aarch64 not found" >&2
    echo "install qemu-system-aarch64/qemu-system-arm and rerun this script" >&2
    exit 127
fi

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
