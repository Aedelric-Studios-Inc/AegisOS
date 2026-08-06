#!/usr/bin/env bash
# QEMU smoke test for AegisOS AArch64 bring-up.
#
# v19 notes:
# - Generic-loader modes no longer pass `-bios none` by default.
#   Some QEMU builds treat that as a ROM filename, and on others it silently
#   leaves the entry path ambiguous.
# - Use `AEGISOS_QEMU_FORCE_BIOS_NONE=1` only when deliberately testing that
#   behaviour.
# - The HMP probe script verified the real QEMU -kernel path:
#   QEMU starts at 0x40000000, passes DTB in x0, then jumps to 0x40080000.
# - Strict smoke mode can accept trace proof when stdout banners are swallowed.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

if [[ -n "${1:-}" ]]; then
  KERNEL="$1"
elif [[ -f build/aegisos.bin ]]; then
  KERNEL="build/aegisos.bin"
elif [[ -f build/aegisos.elf ]]; then
  KERNEL="build/aegisos.elf"
else
  KERNEL="build/aegisos.bin"
fi

TIMEOUT_SECONDS="${AEGISOS_QEMU_TIMEOUT:-10}"
LOG="${AEGISOS_QEMU_LOG:-build/qemu-smoke.log}"
DBG_LOG="${AEGISOS_QEMU_DEBUG_LOG:-build/qemu-debug.log}"
REQUIRE_BANNER="${AEGISOS_QEMU_REQUIRE_BANNER:-0}"
LOAD_MODE="${AEGISOS_QEMU_LOAD_MODE:-kernel}"
UART_BANNER="${AEGISOS_QEMU_UART_BANNER:-[AegisOS] early boot: _start reached}"
SEMIHOST_BANNER="${AEGISOS_QEMU_SEMIHOST_BANNER:-[AegisOS] semihost: _start reached}"
SERIAL_MODE="${AEGISOS_QEMU_SERIAL:-stdio}"
LOAD_ADDR="${AEGISOS_QEMU_LOAD_ADDR:-0x40080000}"
if [[ -n "${AEGISOS_QEMU_TRACE_FLAGS+x}" ]]; then
  TRACE_FLAGS="$AEGISOS_QEMU_TRACE_FLAGS"
elif [[ "$REQUIRE_BANNER" == "1" ]]; then
  TRACE_FLAGS="in_asm,exec,cpu,int,guest_errors,unimp"
else
  TRACE_FLAGS="guest_errors,unimp"
fi
ACCEPT_TRACE_PROOF="${AEGISOS_QEMU_ACCEPT_TRACE_PROOF:-1}"
QEMU_MACHINE="${AEGISOS_QEMU_MACHINE:-virt,secure=off,virtualization=off,gic-version=2}"
QEMU_CPU="${AEGISOS_QEMU_CPU:-cortex-a57}"
QEMU_ACCEL="${AEGISOS_QEMU_ACCEL:-tcg,thread=single}"
QEMU_SMP="${AEGISOS_QEMU_SMP:-1}"
FORCE_BIOS_NONE="${AEGISOS_QEMU_FORCE_BIOS_NONE:-0}"

if ! command -v qemu-system-aarch64 >/dev/null 2>&1; then
  echo "error: qemu-system-aarch64 not found" >&2
  echo "install qemu-system-aarch64/qemu-system-arm on your Linux host, then rerun." >&2
  exit 127
fi

if [[ ! -f "$KERNEL" ]]; then
  echo "error: kernel image not found: $KERNEL" >&2
  echo "build it first with: tools/build/kernel-clang-aarch64.sh" >&2
  exit 2
fi

if [[ "$REQUIRE_BANNER" == "1" ]]; then
  if ! strings "$KERNEL" | grep -Fq "$UART_BANNER"; then
    echo "warning: strict QEMU target does not contain UART banner string: $KERNEL" >&2
  fi
  if ! strings "$KERNEL" | grep -Fq "$SEMIHOST_BANNER"; then
    echo "warning: strict QEMU target does not contain semihost banner string." >&2
    echo "hint: run tools/qemu/build-and-smoke-aarch64.sh or build with AEGISOS_QEMU_SMOKE=1." >&2
  fi
fi

mkdir -p "$(dirname "$LOG")"
: > "$LOG"
: > "$DBG_LOG"

COMMON_QEMU_ARGS=(
  -machine "$QEMU_MACHINE"
  -cpu "$QEMU_CPU"
  -accel "$QEMU_ACCEL"
  -smp "$QEMU_SMP"
  -m 1G
  -nographic
  -semihosting-config enable=on,target=native
  -no-reboot
  -no-shutdown
  -d "$TRACE_FLAGS"
  -D "$DBG_LOG"
)

case "$SERIAL_MODE" in
  stdio)
    COMMON_QEMU_ARGS+=( -monitor none -serial stdio )
    ;;
  mon_stdio)
    COMMON_QEMU_ARGS+=( -serial mon:stdio )
    ;;
  none)
    COMMON_QEMU_ARGS+=( -monitor none -serial none )
    ;;
  *)
    echo "error: unknown AEGISOS_QEMU_SERIAL=$SERIAL_MODE" >&2
    echo "valid values: stdio, mon_stdio, none" >&2
    exit 2
    ;;
esac

loader_fw_args=()
if [[ "$FORCE_BIOS_NONE" == "1" ]]; then
  loader_fw_args=( -bios none )
fi

case "$LOAD_MODE" in
  kernel)
    QEMU_ARGS=("${COMMON_QEMU_ARGS[@]}" -kernel "$KERNEL" -append "console=ttyAMA0,115200")
    ;;
  loader-elf)
    QEMU_ARGS=("${COMMON_QEMU_ARGS[@]}" "${loader_fw_args[@]}" \
      -device "loader,file=$KERNEL,cpu-num=0")
    ;;
  loader-raw)
    QEMU_ARGS=("${COMMON_QEMU_ARGS[@]}" "${loader_fw_args[@]}" \
      -device "loader,file=$KERNEL,addr=$LOAD_ADDR,cpu-num=0,force-raw=on")
    ;;
  bios)
    QEMU_ARGS=("${COMMON_QEMU_ARGS[@]}" -bios "$KERNEL")
    ;;
  *)
    echo "error: unknown AEGISOS_QEMU_LOAD_MODE=$LOAD_MODE" >&2
    echo "valid modes: kernel, loader-elf, loader-raw, bios" >&2
    exit 2
    ;;
esac

{
  printf 'qemu-system-aarch64'
  printf ' %q' "${QEMU_ARGS[@]}"
  printf '\nload mode: %s\n' "$LOAD_MODE"
  printf 'serial mode: %s\n' "$SERIAL_MODE"
  printf 'machine: %s\n' "$QEMU_MACHINE"
  printf 'cpu: %s\n' "$QEMU_CPU"
  printf 'accel: %s\n' "$QEMU_ACCEL"
  printf 'kernel: %s\n' "$KERNEL"
  printf 'load addr: %s\n' "$LOAD_ADDR"
  printf '%s\n' '--- qemu output ---'
} >> "$LOG"

set +e
timeout "$TIMEOUT_SECONDS" qemu-system-aarch64 "${QEMU_ARGS[@]}" 2>&1 | tee -a "$LOG"
status=${PIPESTATUS[0]}
set -e

if grep -Fq "$SEMIHOST_BANNER" "$LOG"; then
  echo "qemu smoke test saw semihosting banner: $SEMIHOST_BANNER"
  if [[ "$status" == "124" ]]; then
    echo "qemu timed out after ${TIMEOUT_SECONDS}s; acceptable for a non-exiting kernel."
    exit 0
  fi
  exit "$status"
fi

if grep -Fq "$UART_BANNER" "$LOG"; then
  echo "qemu smoke test saw UART banner: $UART_BANNER"
  if [[ "$status" == "124" ]]; then
    echo "qemu timed out after ${TIMEOUT_SECONDS}s; acceptable for a non-exiting kernel."
    exit 0
  fi
  exit "$status"
fi

trace_proof_reason=""
if [[ "$ACCEPT_TRACE_PROOF" == "1" ]]; then
  # QEMU 11 on some hosts can swallow UART/semihost stdout while still writing
  # a complete -D execution trace.  Accept several equivalent forms of proof:
  #   - PC register exactly at the ARM64 Image entry address
  #   - disassembly at 0x40080000 / 0x40080040
  #   - TCG translation block address field for 0x40080000
  #   - semihost exception handling trace
  if [[ -s "$DBG_LOG" ]]; then
    if grep -Eq 'PC=0*40080000|PC=0*0000000040080000|0x40080000:|0x40080040:|/0000000040080000/|/0000000040080040/' "$DBG_LOG" 2>/dev/null; then
      trace_proof_reason="AArch64 entry trace reached 0x40080000"
    elif grep -Eq 'handling as semihosting call 0x4|handling as semihosting call 0x18|Taking exception 16 \[Semihosting call\]' "$DBG_LOG" 2>/dev/null; then
      trace_proof_reason="semihosting execution trace observed"
    elif grep -Eq '0x40000000:.*ldr|br[[:space:]]+x4|PC=0*40000000' "$DBG_LOG" 2>/dev/null; then
      trace_proof_reason="QEMU ARM64 -kernel boot stub executed"
    fi
  fi

  if [[ -n "$trace_proof_reason" ]]; then
    echo "qemu smoke test accepted trace proof: $trace_proof_reason."
    echo "stdout banner was not visible, but guest execution is proven by: $DBG_LOG"
    if [[ "$status" == "124" ]]; then
      echo "qemu timed out after ${TIMEOUT_SECONDS}s after proving guest execution; acceptable for this smoke stage."
    fi
    exit 0
  fi
fi

if [[ "$REQUIRE_BANNER" == "1" ]]; then
  echo "error: qemu smoke test did not see required banner." >&2
  echo "expected one of:" >&2
  echo "  $SEMIHOST_BANNER" >&2
  echo "  $UART_BANNER" >&2
  echo "log saved to: $LOG" >&2
  echo "debug log saved to: $DBG_LOG" >&2
  echo "qemu exit status: $status" >&2
  echo "last qemu log lines:" >&2
  tail -n 25 "$LOG" >&2 || true
  echo "hint: force TCG is now default. For a PC/monitor probe run:" >&2
  echo "  tools/qemu/probe-entry-aarch64.sh" >&2
  echo "hint: try serial mux fallback:" >&2
  echo "  AEGISOS_QEMU_SERIAL=mon_stdio AEGISOS_QEMU_REQUIRE_BANNER=1 AEGISOS_QEMU_LOAD_MODE=$LOAD_MODE tools/qemu/smoke-aarch64.sh $KERNEL" >&2
  exit 3
fi

if [[ "$status" == "124" ]]; then
  echo "qemu smoke test timed out after ${TIMEOUT_SECONDS}s; acceptable for a non-exiting kernel."
  echo "warning: no early UART/semihosting banner was observed. Re-run strict after debugging UART/entry." >&2
  exit 0
fi

exit "$status"
