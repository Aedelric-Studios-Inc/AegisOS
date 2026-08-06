#!/usr/bin/env bash
# Build an AegisOS QEMU smoke image and run strict smoke tests.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
: "${AEGISOS_QEMU_TIMEOUT:=14}"

# v55 / AegisOS-v2.0 Release IMG guard: catch stale/extracted-over-old trees before QEMU runs.
tools/dev/assert-v55-milestone.sh


AEGISOS_QEMU_SMOKE=1 AEGISOS_FORCE_REBUILD=1 tools/build/kernel-clang-aarch64.sh

try_smoke() {
  local mode="$1"
  local image="$2"
  local tag="$3"
  echo "== QEMU strict smoke: $tag =="
  AEGISOS_QEMU_REQUIRE_BANNER=1 \
  AEGISOS_QEMU_TIMEOUT="$AEGISOS_QEMU_TIMEOUT" \
  AEGISOS_QEMU_LOAD_MODE="$mode" \
  AEGISOS_QEMU_LOG="build/qemu-smoke-$tag.log" \
  AEGISOS_QEMU_DEBUG_LOG="build/qemu-debug-$tag.log" \
    tools/qemu/smoke-aarch64.sh "$image"
}

prove_and_package_v55() {
  tools/qemu/prove-boot-phases-aarch64.sh build/aegisos.bin build/aegisos.elf
  tools/image/build-aegisos-v2-flash-img.sh --kernel-bin build/aegisos.bin --kernel-elf build/aegisos.elf
  tools/qemu/prove-v40-disk-image-aarch64.sh build/images/AegisOS-v2.0-router-aarch64-v40-flash.img build/aegisos.bin build/aegisos.elf
  tools/image/build-aegisbox-dev-preview-img.sh --kernel-bin build/aegisos.bin --kernel-elf build/aegisos.elf --base-image build/images/AegisOS-v2.0-router-aarch64-v40-flash.img
  tools/image/build-aegisbox-v48-polish-manifest.sh --kernel-bin build/aegisos.bin --kernel-elf build/aegisos.elf --preview-image build/images/developer-preview/AegisOS-v2.0-v47-aegisbox-developer-preview-aarch64.img
  tools/image/build-aegisbox-v55-variant-images.sh --kernel-bin build/aegisos.bin --kernel-elf build/aegisos.elf --base-image build/images/developer-preview/AegisOS-v2.0-v47-aegisbox-developer-preview-aarch64.img
  tools/release/finalize-release-img.sh --kernel-bin build/aegisos.bin --kernel-elf build/aegisos.elf --router-image build/images/variants/AegisOS-v2.0-v55-router-aarch64.img
  tools/release/verify-v55-release.sh
}

# v20: QEMU -kernel with the ARM64 Image binary is the proven path.
# The HMP probe showed QEMU's stub at 0x40000000 jumping into 0x40080000
# with the DTB pointer in x0.  Generic-loader at 0x40080000 overlaps QEMU's
# generated DTB on this host, so loader/bios modes are diagnostic-only now.
if try_smoke kernel build/aegisos.bin kernel-bin; then
  prove_and_package_v55
  exit 0
fi
if try_smoke kernel build/aegisos.elf kernel-elf; then
  prove_and_package_v55
  exit 0
fi

# Last resort: use the HMP/trace proof path against the real AegisOS image.
# This prevents a swallowed serial/semihost banner from failing a kernel whose
# entry path is already proven in QEMU's instruction trace.
if tools/qemu/prove-kernel-entry-aarch64.sh build/aegisos.bin; then
  echo "QEMU -kernel smoke accepted HMP/trace proof for build/aegisos.bin."
  prove_and_package_v55
  exit 0
fi

if [[ "${AEGISOS_QEMU_TRY_UNSAFE_LOADERS:-0}" == "1" ]]; then
  if try_smoke bios build/aegisos.bin bios-bin; then exit 0; fi
  if try_smoke loader-raw build/aegisos.bin loader-raw-no-bios; then exit 0; fi
  if try_smoke loader-elf build/aegisos.elf loader-elf-no-bios; then exit 0; fi
else
  echo "note: skipped loader/bios smoke modes; set AEGISOS_QEMU_TRY_UNSAFE_LOADERS=1 to probe them."
fi

echo "error: QEMU -kernel smoke failed." >&2
echo "upload these logs:" >&2
echo "  build/qemu-smoke-kernel-bin.log" >&2
echo "  build/qemu-debug-kernel-bin.log" >&2
echo "  build/qemu-smoke-kernel-elf.log" >&2
echo "  build/qemu-debug-kernel-elf.log" >&2
exit 3
