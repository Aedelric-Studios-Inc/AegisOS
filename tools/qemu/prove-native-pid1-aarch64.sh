#!/usr/bin/env bash
# Build a normal AegisOS kernel and prove that the embedded /sbin/aegis-init
# ELF executes at EL0 as PID 1 through the AegisOS syscall ABI.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

TIMEOUT_SECONDS="${AEGISOS_QEMU_TIMEOUT:-20}"
LOG="${AEGISOS_QEMU_LOG:-build/qemu-native-pid1.log}"
DTB="${AEGISOS_QEMU_DTB:-build/qemu/aegisos-virt.dtb}"
QEMU_DIAG_LOG="${AEGISOS_QEMU_DIAG_LOG:-build/qemu-native-pid1-qemu.log}"
KERNEL="${AEGISOS_QEMU_KERNEL:-build/aegisos.elf}"
IMG="${AEGISOS_QEMU_IMG:-build/images/release-v55/AegisOS-v2.0-v55-release-aarch64.img}"
CPU="${AEGISOS_QEMU_CPU:-cortex-a57}"
MEMORY="${AEGISOS_QEMU_MEMORY:-1G}"

command -v qemu-system-aarch64 >/dev/null 2>&1 || {
    echo "error: qemu-system-aarch64 not found" >&2
    exit 127
}

command -v timeout >/dev/null 2>&1 || {
    echo "error: timeout not found" >&2
    exit 127
}

AEGISOS_FORCE_REBUILD=1 tools/build/kernel-clang-aarch64.sh

[[ -f "$KERNEL" ]] || {
    echo "error: kernel not found: $KERNEL" >&2
    exit 2
}

# The native PID 1 ELF is embedded in the kernel, but the proof deliberately
# uses the same release-VM topology as the interactive launcher. This prevents
# the test harness from silently exercising a different QEMU configuration.
if [[ ! -f "$IMG" ]]; then
    tools/image/build-aegisos-v2-flash-img.sh
    tools/image/build-aegisbox-dev-preview-img.sh
    tools/image/build-aegisbox-v55-variant-images.sh
    tools/release/finalize-release-img.sh
fi

mkdir -p "$(dirname "$LOG")" "$(dirname "$QEMU_DIAG_LOG")"
: > "$LOG"
: > "$QEMU_DIAG_LOG"

mkdir -p "$(dirname "$DTB")"
if [[ ! -f "$DTB" || "${AEGISOS_REBUILD_DTB:-0}" == "1" ]]; then
  qemu-system-aarch64 -machine "virt,secure=off,virtualization=off,gic-version=2,dumpdtb=$DTB" -cpu "$CPU" -smp 1 -m "$MEMORY" -display none >/dev/null 2>&1
fi

set +e
AEGISOS_SKIP_BUILD=1 \
AEGISOS_QEMU_KERNEL="$KERNEL" \
AEGISOS_QEMU_IMG="$IMG" \
AEGISOS_QEMU_CPU="$CPU" \
AEGISOS_QEMU_MEMORY="$MEMORY" \
AEGISOS_QEMU_SERIAL_LOG="$LOG" \
timeout --signal=TERM --kill-after=2s "$TIMEOUT_SECONDS" \
    tools/qemu/boot-release-vm-aarch64.sh \
    >"$QEMU_DIAG_LOG" 2>&1
qemu_status=$?
set -e

# Print captured guest serial after QEMU closes the file-backed character
# device. QEMU diagnostics remain separate so a launch error cannot be
# mistaken for guest output.
cat "$LOG"

required=(
    "[AegisOS:native] launching /sbin/aegis-init pid=1"
    "[aegis-init] native EL0 PID 1 online under AegisOS"
    "[aegis-init] AegisOS process binding confirmed: pid=1"
    "[aegis-init] service-manager and aegisd kernel registry links confirmed"
    "AegisOS v2.0 v58 native-lifecycle runtime"
)

failed=0
for line in "${required[@]}"; do
    if ! grep -Fq "$line" "$LOG"; then
        echo "error: missing native PID 1 proof line: $line" >&2
        failed=1
    fi
done

if [[ "$failed" != "0" ]]; then
    echo "error: native PID 1 proof failed" >&2
    echo "qemu status: $qemu_status" >&2
    echo "guest serial log: $LOG" >&2
    echo "qemu diagnostic log: $QEMU_DIAG_LOG" >&2
    echo "----- last 120 guest serial lines -----" >&2
    tail -n 120 "$LOG" >&2 || true
    echo "----- QEMU diagnostics -----" >&2
    cat "$QEMU_DIAG_LOG" >&2 || true
    echo "----- end diagnostics -----" >&2
    exit 3
fi

if [[ "$qemu_status" != "0" && "$qemu_status" != "124" ]]; then
    echo "error: qemu exited with status $qemu_status after proof output" >&2
    echo "qemu diagnostics: $QEMU_DIAG_LOG" >&2
    exit "$qemu_status"
fi

echo "AegisOS native PID 1 proof passed."
echo "guest serial log: $LOG"
echo "qemu diagnostic log: $QEMU_DIAG_LOG"
