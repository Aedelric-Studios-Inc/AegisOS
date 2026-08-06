#!/usr/bin/env bash
# Run all PR1 software proofs without flooding the terminal, then check the
# already-prepared software gate.  Full output remains under build/pr1/host-logs.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

HOST_LOG_DIR="${AEGISOS_HOST_LOG_DIR:-build/pr1/host-logs}"
mkdir -p "$HOST_LOG_DIR"

run_proof() {
  local name="$1" timeout_seconds="$2" script="$3"
  local log="$HOST_LOG_DIR/$name.log"
  printf '[PR1] running %s (timeout=%ss)\n' "$name" "$timeout_seconds"
  set +e
  AEGISOS_QEMU_TIMEOUT="$timeout_seconds" "$script" >"$log" 2>&1
  local status=$?
  set -e
  if [[ $status -ne 0 ]]; then
    printf '[PR1] %s FAILED status=%s; tail follows: %s\n' "$name" "$status" "$log" >&2
    tail -n 120 "$log" >&2 || true
    exit "$status"
  fi
  printf '[PR1] %s PASSED; log=%s\n' "$name" "$log"
}

run_proof native-lifecycle "${AEGISOS_LIFECYCLE_TIMEOUT:-75}" tools/qemu/prove-native-lifecycle-aarch64.sh
run_proof runtime "${AEGISOS_RUNTIME_TIMEOUT:-120}" tools/qemu/prove-pr1-runtime-aarch64.sh
run_proof uefi "${AEGISOS_UEFI_TIMEOUT:-60}" tools/qemu/prove-pr1-uefi-aarch64.sh

printf '[PR1] checking software gate without rebuilding product evidence\n'
tools/release/pr1-gate.sh --software-only --no-prepare
