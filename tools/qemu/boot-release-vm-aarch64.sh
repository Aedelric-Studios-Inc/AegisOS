#!/usr/bin/env bash
# Boot the AegisOS v58 native-lifecycle Release IMG in a QEMU AArch64 VM.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

KERNEL="${AEGISOS_QEMU_KERNEL:-build/aegisos.bin}"
IMG="${AEGISOS_QEMU_IMG:-build/images/release-v55/AegisOS-v2.0-v55-release-aarch64.img}"
MEMORY="${AEGISOS_QEMU_MEMORY:-1G}"
CPU="${AEGISOS_QEMU_CPU:-cortex-a57}"
SERIAL_LOG="${AEGISOS_QEMU_SERIAL_LOG:-}"
DTB="${AEGISOS_QEMU_DTB:-}"

# Important: do not build with AEGISOS_QEMU_SMOKE=1 here. The smoke kernel runs
# the proof EL0 path and exits/hangs for trace validation. The release VM uses a
# normal non-smoke kernel so native PID 1 can run before the interactive console.
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

# A raw ARM64 Image is required here.  QEMU's ELF loader jumps to the ELF
# entry but leaves x0 as zero, which loses the firmware FDT pointer.  With the
# Image header in aegisos.bin, QEMU generates the final machine DTB after all
# virtio devices are attached and passes its address in x0.
dtb_args=()
if [[ -n "$DTB" ]]; then
  [[ -f "$DTB" ]] || { echo "error: requested DTB not found: $DTB" >&2; exit 2; }
  dtb_args=(-dtb "$DTB")
fi

serial_args=()
if [[ -n "$SERIAL_LOG" ]]; then
  mkdir -p "$(dirname "$SERIAL_LOG")"
  : > "$SERIAL_LOG"
  serial_args=(
    -display none
    -monitor none
    -chardev "file,id=aegisserial,path=$SERIAL_LOG"
    -serial chardev:aegisserial
  )
else
  serial_args=(
    -nographic
    -serial mon:stdio
  )
fi

exec qemu-system-aarch64 \
  -machine virt,secure=off,virtualization=off,gic-version=2 \
  -cpu "$CPU" \
  -smp 1 \
  -m "$MEMORY" \
  -global virtio-mmio.force-legacy=false \
  "${serial_args[@]}" \
  -semihosting-config enable=on,target=native \
  -kernel "$KERNEL" \
  "${dtb_args[@]}" \
  -drive file="$IMG",format=raw,if=none,id=aegisflash \
  -device virtio-blk-device,drive=aegisflash \
  -netdev user,id=net0 \
  -device virtio-net-device,netdev=net0 \
  -object rng-random,filename=/dev/urandom,id=rng0 \
  -device virtio-rng-device,rng=rng0 \
  -append "console=ttyAMA0,115200 aegis.interactive=1"
