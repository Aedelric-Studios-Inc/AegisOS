#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
IMAGE="${1:-build/images/AegisOS-v2.0-router-aarch64-v40-flash.img}"
KERNEL="${AEGISOS_QEMU_KERNEL:-build/aegisos.bin}"
if [[ ! -f "$IMAGE" ]]; then
  echo "error: v40 flash image not found: $IMAGE" >&2
  echo "run: tools/image/build-aegisos-v2-flash-img.sh" >&2
  exit 2
fi
if [[ ! -f "$KERNEL" ]]; then
  echo "error: kernel not found: $KERNEL" >&2
  echo "run: AEGISOS_QEMU_SMOKE=1 AEGISOS_FORCE_REBUILD=1 tools/build/kernel-clang-aarch64.sh" >&2
  exit 2
fi
exec qemu-system-aarch64 \
  -machine "${AEGISOS_QEMU_MACHINE:-virt,secure=off,virtualization=off,gic-version=2}" \
  -cpu "${AEGISOS_QEMU_CPU:-cortex-a57}" \
  -accel "${AEGISOS_QEMU_ACCEL:-tcg,thread=single}" \
  -smp 1 -m 1G -nographic \
  -semihosting-config enable=on,target=native \
  -no-reboot -no-shutdown \
  -monitor none -serial stdio \
  -drive if=none,file="$IMAGE",format=raw,id=aegisflash \
  -device virtio-blk-device,drive=aegisflash \
  -kernel "$KERNEL" -append "console=ttyAMA0,115200 aegisos.flash=/dev/vda aegisos.config=AEGIS_CONFIG"
