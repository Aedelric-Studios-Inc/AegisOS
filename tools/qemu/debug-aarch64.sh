#!/usr/bin/env bash
# Run AegisOS in QEMU with GDB server for debugging.
set -euo pipefail

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
