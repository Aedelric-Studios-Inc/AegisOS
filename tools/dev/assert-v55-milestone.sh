#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
need_file() { [[ -f "$1" ]] || { echo "missing: $1" >&2; exit 2; }; }
need_grep() { local pattern="$1" file="$2"; grep -Fq "$pattern" "$file" || { echo "missing pattern in $file: $pattern" >&2; exit 2; }; }
# Direct v48 inheritance checks, without requiring VERSION to remain at v48.
need_file kernel/include/release_polish.h
need_file kernel/core/release_polish.c
need_file tools/image/build-aegisbox-v48-polish-manifest.py
need_file tools/image/build-aegisbox-v48-polish-manifest.sh
need_grep 'release_polish_prepare_v48' kernel/core/release_polish.c
need_grep 'qemu_boot_checkpoint_v48_image_polish_manifest_ready' kernel/core/kernel_main.c
# v55 release checks.
need_file kernel/include/release_final.h
need_file kernel/core/release_final.c
need_file tools/image/build-aegisbox-v55-variant-images.py
need_file tools/image/build-aegisbox-v55-variant-images.sh
need_file tools/release/finalize-release-img.py
need_file tools/release/finalize-release-img.sh
need_file tools/release/verify-v55-release.sh
need_file docs/V50_V55_RELEASE_NOTES.md
need_file docs/KNOWN_LIMITATIONS.md
need_file docs/HARDWARE_NOTES.md
need_file docs/FLASHING_RELEASE_IMG.md
need_grep 'release_final_prepare_v55' kernel/core/release_final.c
need_grep 'aegisbox_init_prepare_v55_release_or_panic' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_v50_installer_flash_flow_hardened' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_v51_variant_images_ready' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_v52_security_service_defaults_frozen' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_v53_docs_known_limits_hardware_notes_ready' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_v54_final_rc_audit_signing_checksums_ready' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_v55_release_img_ready' kernel/core/kernel_main.c
need_grep 'QEMU v55 proof accepted' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'AegisOS-v2.0-v55-release-img' VERSION
echo "v55 milestone guard: OK (AegisOS-v2.0-v55-release-img)"
