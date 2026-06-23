#!/usr/bin/env bash
# Produce a short instruction trace for the standalone sanity firmware.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

# Ensure sanity images exist.  Do not care if QEMU probe fails here.
AEGISOS_QEMU_TIMEOUT=1 tools/qemu/sanity-aarch64.sh >/dev/null 2>&1 || true

TRACE_LOG="build/qemu-sanity/trace-loader-raw-bare.log"
RUN_LOG="build/qemu-sanity/trace-loader-raw-bare.run.log"
: > "$TRACE_LOG"
: > "$RUN_LOG"
KERNEL="build/qemu-sanity/sanity_semihost_exit_ram.bin"
if [[ ! -f "$KERNEL" ]]; then
  echo "error: missing $KERNEL" >&2
  exit 2
fi

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
  -device "loader,file=$KERNEL,addr=0x40080000,cpu-num=0,force-raw=on" \
  -d in_asm,exec,cpu,int,guest_errors,unimp \
  -D "$TRACE_LOG" \
  2>&1 | tee "$RUN_LOG"
status=${PIPESTATUS[0]}
set -e

echo "qemu exit status: $status"
echo "trace log: $TRACE_LOG"
echo "run log: $RUN_LOG"
echo "first trace lines:"
sed -n '1,160p' "$TRACE_LOG" || true
