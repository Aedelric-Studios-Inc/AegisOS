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

tools/dev/assert-v47-milestone.sh >/dev/null
need_file kernel/include/release_polish.h
need_file kernel/core/release_polish.c
need_file tools/image/build-aegisbox-v48-polish-manifest.py
need_file tools/image/build-aegisbox-v48-polish-manifest.sh
need_grep 'release_polish_prepare_v48' kernel/core/release_polish.c
need_grep 'aegisbox_init_prepare_v48_release_polish_or_panic' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_v48_board_profile_polish_ready' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_v48_service_config_matrix_ready' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_v48_security_hardening_ready' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_v48_image_polish_manifest_ready' kernel/core/kernel_main.c
need_grep 'QEMU v48 proof accepted' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'AegisOS-v2.0-v48-release-polish-board-profiles-service-configs' VERSION

echo "v48 milestone guard: OK (AegisOS-v2.0-v48-release-polish-board-profiles-service-configs)"
