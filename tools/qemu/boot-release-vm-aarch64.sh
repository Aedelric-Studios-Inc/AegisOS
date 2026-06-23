#!/usr/bin/env bash
# Boot the AegisOS v55.1 Release IMG in a QEMU AArch64 VM with the interactive serial console.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

KERNEL="${AEGISOS_QEMU_KERNEL:-build/aegisos.elf}"
IMG="${AEGISOS_QEMU_IMG:-build/images/release-v55/AegisOS-v2.0-v55-release-aarch64.img}"
MEMORY="${AEGISOS_QEMU_MEMORY:-1G}"
CPU="${AEGISOS_QEMU_CPU:-cortex-a57}"

# Important: do not build with AEGISOS_QEMU_SMOKE=1 here. The smoke kernel runs
# the proof EL0 path and exits/hangs for trace validation. The release VM uses a
# normal non-smoke kernel so v55.1 can start the interactive console.
if [[ "${AEGISOS_SKIP_BUILD:-0}" != "1" ]]; then
  AEGISOS_FORCE_REBUILD=1 tools/build/kernel-clang-aarch64.sh
  tools/image/build-aegisos-v2-flash-img.sh
  tools/image/build-aegisbox-dev-preview-img.sh
  tools/image/build-aegisbox-v55-variant-images.sh
  tools/release/finalize-release-img.sh
fi

if [[ ! -f "$KERNEL" ]]; then
  echo "error: kernel not found: $KERNEL" >&2
  exit 2
fi

if [[ ! -f "$IMG" ]]; then
  echo "error: Release IMG not found: $IMG" >&2
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
  -append "console=ttyAMA0,115200 aegis.interactive=1"
