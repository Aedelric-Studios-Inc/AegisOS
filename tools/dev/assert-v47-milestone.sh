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

tools/dev/assert-v46-milestone.sh >/dev/null
need_file kernel/include/developer_preview.h
need_file kernel/core/developer_preview.c
need_file tools/image/build-aegisbox-dev-preview-img.py
need_file tools/image/build-aegisbox-dev-preview-img.sh
need_file tools/flashing/flash-aegisbox-dev-preview.sh
need_grep 'developer_preview_prepare_v47_img' kernel/core/developer_preview.c
need_grep 'qemu_boot_checkpoint_v47_dev_preview_manifest_ready' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_v47_board_profiles_ready' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_v47_installer_flash_flow_ready' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_v47_developer_preview_img_ready' kernel/core/kernel_main.c
need_grep 'QEMU v47 proof accepted' tools/qemu/prove-boot-phases-aarch64.sh
if ! grep -Fq 'AegisOS-v2.0-v47-aegisbox-developer-preview-img' VERSION && \
   ! grep -Fq 'AegisOS-v2.0-v48-release-polish-board-profiles-service-configs' VERSION; then
  echo 'missing v47/v48 milestone version marker in VERSION' >&2
  exit 2
fi

echo "v47 milestone guard: OK (AegisOS-v2.0-v47-aegisbox-developer-preview-img)"
