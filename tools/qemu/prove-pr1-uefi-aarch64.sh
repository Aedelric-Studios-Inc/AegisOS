#!/usr/bin/env bash
# Prove that the PR1 disk reaches AegisOS through UEFI without QEMU -kernel.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

TIMEOUT_SECONDS="${AEGISOS_QEMU_TIMEOUT:-90}"
DISK="${AEGISOS_PR1_DISK:-build/images/pr1/AegisOS-2.0.0-pre.1-aarch64-uefi.img}"
LOG="${AEGISOS_QEMU_LOG:-build/pr1/proofs/uefi-boot.log}"
DIAG="${AEGISOS_QEMU_DIAG_LOG:-build/pr1/proofs/uefi-boot-qemu.log}"

absolute_path() {
  case "$1" in
    /*) printf '%s\n' "$1" ;;
    *) printf '%s/%s\n' "$ROOT" "$1" ;;
  esac
}

DISK="$(absolute_path "$DISK")"
LOG="$(absolute_path "$LOG")"
DIAG="$(absolute_path "$DIAG")"

command -v qemu-system-aarch64 >/dev/null || {
  echo "error: qemu-system-aarch64 not found" >&2
  exit 127
}

command -v timeout >/dev/null || {
  echo "error: timeout not found" >&2
  exit 127
}

if [[ "${AEGISOS_SKIP_BUILD:-0}" != 1 ]]; then
  tools/image/build-pr1-uefi-disk.sh \
    --board "${BOARD:-bastion}" \
    --out "$DISK" \
    --size-mib "${AEGISOS_PR1_DISK_MIB:-256}"
fi

[[ -f "$DISK" ]] || {
  echo "error: UEFI disk not found: $DISK" >&2
  exit 2
}

CODE="${AEGISOS_UEFI_CODE:-/usr/share/AAVMF/AAVMF_CODE.fd}"
VARS="${AEGISOS_UEFI_VARS:-/usr/share/AAVMF/AAVMF_VARS.fd}"
BLOCK_DEVICE="${AEGISOS_UEFI_BLOCK_DEVICE:-virtio-blk-device}"
MACHINE="${AEGISOS_QEMU_MACHINE:-virt,secure=off,virtualization=off,gic-version=2,acpi=off}"
MEMORY="${AEGISOS_QEMU_MEMORY:-8G}"
VARS_COPY="$ROOT/build/pr1/uefi-vars-proof.fd"

[[ -f "$CODE" ]] || {
  echo "error: UEFI firmware not found: $CODE" >&2
  exit 2
}

[[ -f "$VARS" ]] || {
  echo "error: UEFI variable store not found: $VARS" >&2
  exit 2
}

mkdir -p "$(dirname "$LOG")" "$(dirname "$DIAG")" "$(dirname "$VARS_COPY")"
rm -f "$VARS_COPY"
: > "$LOG"

{
  echo "[AegisOS:uefi-proof] timeout-seconds=$TIMEOUT_SECONDS"
  echo "[AegisOS:uefi-proof] disk=$DISK"
  echo "[AegisOS:uefi-proof] firmware=$CODE"
  echo "[AegisOS:uefi-proof] vars=$VARS"
  echo "[AegisOS:uefi-proof] vars-copy=$VARS_COPY"
  echo "[AegisOS:uefi-proof] block-device=$BLOCK_DEVICE"
  echo "[AegisOS:uefi-proof] machine=$MACHINE"
  echo "[AegisOS:uefi-proof] memory=$MEMORY"
} > "$DIAG"

set +e
AEGISOS_SKIP_BUILD=1 \
AEGISOS_PR1_DISK="$DISK" \
AEGISOS_UEFI_CODE="$CODE" \
AEGISOS_UEFI_VARS="$VARS" \
AEGISOS_UEFI_VARS_COPY="$VARS_COPY" \
AEGISOS_UEFI_BLOCK_DEVICE="$BLOCK_DEVICE" \
AEGISOS_QEMU_MACHINE="$MACHINE" \
AEGISOS_QEMU_MEMORY="$MEMORY" \
timeout --foreground --signal=TERM --kill-after=2s "$TIMEOUT_SECONDS" \
  ./tools/qemu/boot-pr1-uefi-aarch64.sh > "$LOG" 2>&1
status=$?
set -e

echo "[AegisOS:uefi-proof] qemu-status=$status" >> "$DIAG"

required=(
  "[AegisOS:uefi] loader entered"
  "[AegisOS:uefi] kernel load region reserved"
  "[AegisOS:uefi] ExitBootServices complete"
  "[AegisOS:uefi] handing off to EL1"
  "[AegisOS] early boot: _start reached"
  "[AegisOS:dtb] detected FDT"
  "[AegisOS:product] board="
)

failed=0

for line in "${required[@]}"; do
  if ! grep -Fq "$line" "$LOG"; then
    echo "error: missing UEFI proof line: $line" >&2
    failed=1
  fi
done

if ((failed)); then
  echo "error: PR1 UEFI boot proof failed; qemu status=$status" >&2
  tail -n 240 "$LOG" >&2 || true
  cat "$DIAG" >&2 || true
  exit 3
fi

if [[ "$status" != 0 && "$status" != 124 ]]; then
  echo "error: QEMU exited unexpectedly with status=$status" >&2
  exit "$status"
fi

printf 'AegisOS PR1 UEFI disk boot proof passed.\n' | tee -a "$LOG"
printf 'guest serial log: %s\n' "$LOG"
printf 'qemu diagnostic log: %s\n' "$DIAG"
