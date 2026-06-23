#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
need_file() { [[ -f "$1" ]] || { echo "error: v43 guard failed: missing $1" >&2; exit 2; }; }
need_grep() { grep -q "$1" "$2" || { echo "error: v43 guard failed: missing '$1' in $2" >&2; exit 2; }; }
if [[ ! -f VERSION ]] || ! grep -Eq 'AegisOS-v2\.0-v43(\.[1234])?-.*ipc' VERSION; then
  echo "error: v43 guard failed: stale or wrong VERSION" >&2
  exit 2
fi
need_file kernel/include/ipc_service.h
need_file kernel/ipc/service_ipc.c
need_file docs/IPC_SERVICE_MESSAGING.md
need_file docs/V43_RELEASE_NOTES.md
need_file docs/V43_2_PATCH_NOTES.md
need_file docs/V43_3_PATCH_NOTES.md
need_grep 'service_ipc_prepare_service_messaging' kernel/core/kernel_main.c
need_grep 'service_ipc_message_roundtrip_selftest' kernel/core/kernel_main.c
need_grep 'SYS_SEND_MSG' kernel/include/syscalls.h
need_grep 'SYS_RECV_MSG' kernel/include/syscalls.h
need_grep 'qemu_boot_checkpoint_ipc_service_messaging_ready' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'qemu_boot_checkpoint_ipc_message_roundtrip_ready' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'QEMU v43.4 proof accepted' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'assert-v43-milestone.sh' tools/qemu/build-and-smoke-aarch64.sh
if compgen -G 'README-v*.md' >/dev/null; then
  echo "error: v43 guard failed: root milestone README-v*.md files should remain consolidated" >&2
  exit 2
fi
echo "v43.4 milestone guard: OK (AegisOS-v2.0-v43.4-ipc-diagnostic-direct-mailbox-fix)"
