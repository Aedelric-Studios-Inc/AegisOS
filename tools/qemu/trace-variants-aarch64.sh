#!/usr/bin/env bash
# Run several tiny QEMU trace probes without requiring banner success.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

AEGISOS_QEMU_TIMEOUT=1 tools/qemu/sanity-aarch64.sh >/dev/null 2>&1 || true
mkdir -p build/qemu-sanity

run_probe() {
  local tag="$1"; shift
  local trace="build/qemu-sanity/trace-${tag}.log"
  local run="build/qemu-sanity/trace-${tag}.run.log"
  : > "$trace"; : > "$run"
  echo "== trace probe: $tag =="
  set +e
  timeout 3 qemu-system-aarch64 \
    -machine "${AEGISOS_QEMU_MACHINE:-virt,secure=off,virtualization=off,gic-version=2}" \
    -cpu "${AEGISOS_QEMU_CPU:-cortex-a57}" \
    -accel "${AEGISOS_QEMU_ACCEL:-tcg,thread=single}" \
    -smp 1 \
    -m 1G \
    -nographic \
    -semihosting-config enable=on,target=native \
    -no-reboot -no-shutdown \
    -monitor none -serial stdio \
    -d in_asm,exec,cpu,int,guest_errors,unimp \
    -D "$trace" \
    "$@" \
    2>&1 | tee "$run"
  local status=${PIPESTATUS[0]}
  set -e
  echo "qemu exit status: $status"
  echo "trace log: $trace"
  echo "run log: $run"
  echo "first trace lines:"
  sed -n '1,100p' "$trace" || true
}

run_probe semihost-loader-raw-no-bios \
  -device loader,file=build/qemu-sanity/sanity_semihost_exit_ram.bin,addr=0x40080000,cpu-num=0,force-raw=on
run_probe semihost-loader-elf-no-bios \
  -device loader,file=build/qemu-sanity/sanity_semihost_exit_ram.elf,cpu-num=0
run_probe semihost-bios \
  -bios build/qemu-sanity/sanity_semihost_exit_bios.bin
run_probe uart-loader-raw-no-bios \
  -device loader,file=build/qemu-sanity/sanity_ram.bin,addr=0x40080000,cpu-num=0,force-raw=on
run_probe kernel-image-bin \
  -kernel build/qemu-sanity/sanity_image.bin \
  -append console=ttyAMA0,115200
