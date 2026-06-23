#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
IMAGE="${1:-build/images/AegisOS-v2.0-router-aarch64-v40-flash.img}"
KERNEL="${2:-build/aegisos.bin}"
ELF="${3:-build/aegisos.elf}"
TIMEOUT_SECONDS="${AEGISOS_QEMU_TIMEOUT:-8}"
TRACE="build/qemu-proof/v40-disk-image.trace.log"
RUN="build/qemu-proof/v40-disk-image.run.log"
mkdir -p build/qemu-proof
if [[ ! -f "$IMAGE" ]]; then echo "error: missing v40 image: $IMAGE" >&2; exit 2; fi
if [[ ! -f "$KERNEL" ]]; then echo "error: missing kernel: $KERNEL" >&2; exit 2; fi
if [[ ! -f "$ELF" ]]; then echo "error: missing ELF: $ELF" >&2; exit 2; fi
command -v qemu-system-aarch64 >/dev/null 2>&1 || { echo "error: qemu-system-aarch64 not found" >&2; exit 127; }
command -v llvm-nm >/dev/null 2>&1 || command -v nm >/dev/null 2>&1 || { echo "error: nm/llvm-nm not found" >&2; exit 127; }
NM="$(command -v llvm-nm || command -v nm)"
sym_addr() { "$NM" -n "$ELF" | awk -v s="$1" '$3==s {print $1; exit}'; }
require_sym() { local a; a="$(sym_addr "$1")"; [[ -n "$a" ]] || { echo "error: missing symbol $1" >&2; exit 2; }; printf '%016x' "$((16#$a))"; }
START_ADDR="$(require_sym _start)"
KMAIN_ADDR="$(require_sym kernel_main)"
PHASE_ADDR="$(require_sym boot_phase_enter)"
qemu-system-aarch64 \
  -machine "${AEGISOS_QEMU_MACHINE:-virt,secure=off,virtualization=off,gic-version=2}" \
  -cpu "${AEGISOS_QEMU_CPU:-cortex-a57}" \
  -accel "${AEGISOS_QEMU_ACCEL:-tcg,thread=single}" \
  -smp 1 -m 1G -nographic \
  -semihosting-config enable=on,target=native \
  -no-reboot -no-shutdown \
  -monitor none -serial none \
  -drive if=none,file="$IMAGE",format=raw,id=aegisflash \
  -device virtio-blk-device,drive=aegisflash \
  -d in_asm,exec,cpu,int,guest_errors,unimp \
  -D "$TRACE" \
  -kernel "$KERNEL" -append "console=ttyAMA0,115200 aegisos.flash=/dev/vda aegisos.config=AEGIS_CONFIG" \
  >"$RUN" 2>&1 &
QPID=$!
sleep 0.2
set +e
timeout "$TIMEOUT_SECONDS" tail --pid="$QPID" -f /dev/null >/dev/null 2>&1
set -e
kill "$QPID" 2>/dev/null || true
wait "$QPID" 2>/dev/null || true
contains_addr() { local short; short="$(echo "$1" | sed 's/^0*//')"; [[ -n "$short" ]] || short="0"; grep -Eiq "PC=0*${short}|0x${short}:|/0*${short}/" "$TRACE"; }
for pair in "_start:$START_ADDR" "kernel_main:$KMAIN_ADDR" "boot_phase_enter:$PHASE_ADDR"; do
  sym="${pair%%:*}"; addr="${pair#*:}"
  if contains_addr "$addr"; then
    echo "qemu v40 disk proof: reached ${sym} at 0x${addr}."
  else
    echo "error: v40 disk image proof did not reach ${sym} at 0x${addr}" >&2
    echo "run log: $RUN" >&2
    echo "trace log: $TRACE" >&2
    exit 1
  fi
done
python3 tools/image/inspect-aegisos-v2-flash-img.py "$IMAGE" >/dev/null
echo "QEMU v40 disk/image boot path accepted: kernel booted with flash image attached: $IMAGE"
echo "trace log: $TRACE"
