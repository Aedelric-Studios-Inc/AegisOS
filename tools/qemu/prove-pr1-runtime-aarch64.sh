#!/usr/bin/env bash
# Build the native Rust userspace and prove the PR1 software runtime in QEMU.
# This does not satisfy physical NVMe/NIC, power-loss, soak, or external audit gates.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
TIMEOUT_SECONDS="${AEGISOS_QEMU_TIMEOUT:-60}"
LOG="${AEGISOS_QEMU_LOG:-build/pr1/proofs/pr1-runtime.log}"
DIAG="${AEGISOS_QEMU_DIAG_LOG:-build/pr1/proofs/pr1-runtime-qemu.log}"
IMG="${AEGISOS_QEMU_IMG:-build/images/release-v55/AegisOS-v2.0-v55-release-aarch64.img}"

command -v cargo >/dev/null || { echo "error: Cargo nightly is required" >&2; exit 127; }
command -v qemu-system-aarch64 >/dev/null || { echo "error: qemu-system-aarch64 not found" >&2; exit 127; }
command -v timeout >/dev/null || { echo "error: timeout not found" >&2; exit 127; }

NATIVE_USER_IMPL=rust AEGISOS_FORCE_REBUILD=1 tools/build/kernel-clang-aarch64.sh
for marker in \
  "[aegis-init:rust] native Rust PID 1 online under AegisOS" \
  "[service-manager:rust] native Rust supervisor online" \
  "[aegisd:rust] native Rust daemon online" \
  "[dashboard:rust] listening on 0.0.0.0:8080" \
  "[rustmyadmin:rust] listening on 0.0.0.0:8081"; do
  grep -aFq "$marker" build/aegisos.elf || {
    echo "error: Rust kernel payload validation failed; missing marker: $marker" >&2
    exit 3
  }
done
[[ -f "$IMG" ]] || {
  tools/image/build-aegisos-v2-flash-img.sh
  tools/image/build-aegisbox-dev-preview-img.sh
  tools/image/build-aegisbox-v55-variant-images.sh
  tools/release/finalize-release-img.sh
}
mkdir -p "$(dirname "$LOG")"
: > "$LOG"; : > "$DIAG"
set +e
AEGISOS_SKIP_BUILD=1 \
AEGISOS_QEMU_SERIAL_LOG="$LOG" \
AEGISOS_QEMU_KERNEL=build/aegisos.bin \
AEGISOS_QEMU_IMG="$IMG" \
timeout --signal=TERM --kill-after=2s "$TIMEOUT_SECONDS" \
  tools/qemu/boot-release-vm-aarch64.sh >"$DIAG" 2>&1
status=$?
set -e

# Materialise the master guest log before validation.  A partial runtime run
# must still preserve the Rust/userspace evidence that actually executed; the
# gate can then report only the subsystems that genuinely remain unproved.
cp "$LOG" build/pr1/proofs/persistence.log
cp "$LOG" build/pr1/proofs/network.log
cp "$LOG" build/pr1/proofs/socket.log
cp "$LOG" build/pr1/proofs/rust-userspace.log
cp "$LOG" build/pr1/proofs/rustmyadmin.log

required=(
  "[AegisOS:dtb] detected FDT"
  "[AegisOS:storage] AegisFS persistence proof passed"
  "[AegisOS:network] DHCP bound"
  "[AegisOS:network] native socket proof passed"
  "[aegis-init:rust] native Rust PID 1 online under AegisOS"
  "[service-manager:rust] native Rust supervisor online"
  "[aegisd:rust] native Rust daemon online"
  "[service-manager:rust] aegisd IPC health confirmed"
  "[dashboard:rust] listening on 0.0.0.0:8080"
  "[rustmyadmin:rust] listening on 0.0.0.0:8081"
  "aegis:/#"
)
failed=0
for line in "${required[@]}"; do
  grep -Fq "$line" "$LOG" || { echo "error: missing PR1 runtime proof line: $line" >&2; failed=1; }
done
if ((failed)); then
  echo "error: PR1 software runtime proof failed; qemu status=$status" >&2
  tail -n 260 "$LOG" >&2 || true
  cat "$DIAG" >&2 || true
  exit 3
fi
if [[ "$status" != 0 && "$status" != 124 ]]; then exit "$status"; fi

printf 'AegisOS PR1 QEMU software runtime proof passed.\n'
