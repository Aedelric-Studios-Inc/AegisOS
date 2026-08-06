#!/usr/bin/env bash
# Build a normal AegisOS kernel and prove the v58 native lifecycle chain:
# PID 3 exits with status 42, PID 2 reaps it, releases its runtime resources,
# and restarts aegisd as PID 4 before releasing the foreground shell.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

TIMEOUT_SECONDS="${AEGISOS_QEMU_TIMEOUT:-30}"
LOG="${AEGISOS_QEMU_LOG:-build/pr1/proofs/native-lifecycle.log}"
QEMU_DIAG_LOG="${AEGISOS_QEMU_DIAG_LOG:-build/pr1/proofs/native-lifecycle-qemu.log}"
KERNEL="${AEGISOS_QEMU_KERNEL:-build/aegisos.bin}"
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

EXTRA_CFLAGS="${EXTRA_CFLAGS:-} -DAEGISOS_V58_LIFECYCLE_PROOF=1" \
AEGISOS_FORCE_REBUILD=1 tools/build/kernel-clang-aarch64.sh

[[ -f "$KERNEL" ]] || {
    echo "error: kernel not found: $KERNEL" >&2
    exit 2
}

if [[ ! -f "$IMG" ]]; then
    tools/image/build-aegisos-v2-flash-img.sh
    tools/image/build-aegisbox-dev-preview-img.sh
    tools/image/build-aegisbox-v55-variant-images.sh
    tools/release/finalize-release-img.sh
fi

mkdir -p "$(dirname "$LOG")" "$(dirname "$QEMU_DIAG_LOG")"
: > "$LOG"
: > "$QEMU_DIAG_LOG"

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

required=(
    "[AegisOS:userland] aegis-init pid=1 state=native-running"
    "[aegis-init] spawned service-manager pid=2"
    "[AegisOS:supervisor] service-manager pid=2 state=native-running"
    "[service-manager] spawned aegisd pid=3"
    "[AegisOS:supervisor] aegisd pid=3 state=native-running"
    "[aegisd] deliberate lifecycle test exit status=42"
    "[service-manager] reaped aegisd pid=3 status=42"
    "[service-manager] restarting aegisd attempt=1"
    "[service-manager] restarted aegisd pid=4"
    "[AegisOS:supervisor] aegisd pid=4 state=native-running"
    "[service-manager] aegisd recovery IPC health check passed"
    "[AegisOS:console] supervisor recovery health confirmed; releasing ttyAMA0 shell"
    "AegisOS v2.0 v58 native-lifecycle runtime"
    "aegis:/#"
)

failed=0
for line in "${required[@]}"; do
    if ! grep -Fq "$line" "$LOG"; then
        echo "error: missing native lifecycle proof line: $line" >&2
        failed=1
    fi
done

if [[ "$failed" != "0" ]]; then
    echo "error: native lifecycle proof failed" >&2
    echo "qemu status: $qemu_status" >&2
    echo "guest serial log: $LOG" >&2
    echo "qemu diagnostic log: $QEMU_DIAG_LOG" >&2
    echo "----- last 220 guest serial lines -----" >&2
    tail -n 220 "$LOG" >&2 || true
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

printf 'AegisOS v58 native lifecycle/recovery proof passed.\n' | tee -a "$LOG"
echo "guest serial log: $LOG"
echo "qemu diagnostic log: $QEMU_DIAG_LOG"
