#!/usr/bin/env bash
# Run AegisOS in QEMU with GDB server for debugging.
set -euo pipefail

if ! command -v qemu-system-aarch64 >/dev/null 2>&1; then
    echo "error: qemu-system-aarch64 not found" >&2
    echo "install qemu-system-aarch64/qemu-system-arm and rerun this script" >&2
    exit 127
fi

KERNEL="${1:-build/aegisos.bin}"

qemu-system-aarch64 \
    -machine virt \
    -cpu cortex-a57 \
    -m 1G \
    -nographic \
    -serial mon:stdio \
    -kernel "$KERNEL" \
    -append "console=ttyAMA0,115200" \
    -S -gdb tcp::1234 &

echo "[debug] Waiting for GDB on port 1234..."
echo "[debug] Run: aarch64-linux-gnu-gdb -ex 'target remote :1234' $KERNEL"
