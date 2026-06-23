#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

need_file() {
  [[ -f "$1" ]] || { echo "missing: $1" >&2; exit 2; }
}
need_grep() {
  local pattern="$1" file="$2"
  grep -Fq "$pattern" "$file" || { echo "missing pattern in $file: $pattern" >&2; exit 2; }
}

need_file kernel/include/service_supervisor.h
need_file kernel/core/service_supervisor.c
need_grep 'service_supervisor_prepare_v44_fault_proof' kernel/core/service_supervisor.c
need_grep 'exception_decode_fault' kernel/core/exception.c
need_grep 'qemu_boot_checkpoint_v44_service_supervisor_ready' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_v44_page_fault_trap_recorded' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_v44_service_marked_faulted' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_v44_supervisor_handoff_preserved' kernel/core/kernel_main.c
need_grep 'QEMU v44 proof accepted' tools/qemu/prove-boot-phases-aarch64.sh

echo "v44 milestone guard: OK (AegisOS-v2.0-v44-service-supervisor-fault-proof)"
