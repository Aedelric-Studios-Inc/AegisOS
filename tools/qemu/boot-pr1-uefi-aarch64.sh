#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

CODE="${AEGISOS_UEFI_CODE:-}"
if [[ -z "$CODE" ]]; then
  for p in \
    /usr/share/AAVMF/AAVMF_CODE.fd \
    /usr/share/edk2/aarch64/QEMU_EFI.fd \
    /usr/share/qemu-efi-aarch64/QEMU_EFI.fd; do
    if [[ -f "$p" ]]; then CODE="$p"; break; fi
  done
fi
[[ -n "$CODE" && -f "$CODE" ]] || {
  echo "error: ARM64 UEFI firmware not found; set AEGISOS_UEFI_CODE" >&2
  exit 2
}

DISK="${AEGISOS_PR1_DISK:-build/images/pr1/AegisOS-2.0.0-pre.1-aarch64-uefi.img}"
if [[ "${AEGISOS_SKIP_BUILD:-0}" != 1 ]]; then
  tools/image/build-pr1-uefi-disk.sh \
    --board "${BOARD:-bastion}" \
    --out "$DISK" \
    --size-mib "${AEGISOS_PR1_DISK_MIB:-256}"
fi
[[ -f "$DISK" ]] || { echo "error: UEFI disk not found: $DISK" >&2; exit 2; }

firmware_args=()
firmware_mode="bios"
vars_copy=""
case "$(basename "$CODE")" in
  *CODE*.fd|*CODE*.bin)
    VARS="${AEGISOS_UEFI_VARS:-}"
    if [[ -z "$VARS" ]]; then
      code_dir="$(dirname "$CODE")"
      code_base="$(basename "$CODE")"
      for candidate in \
        "$code_dir/${code_base/CODE/VARS}" \
        /usr/share/AAVMF/AAVMF_VARS.fd \
        /usr/share/edk2/aarch64/QEMU_VARS.fd; do
        if [[ -f "$candidate" ]]; then VARS="$candidate"; break; fi
      done
    fi
    [[ -n "$VARS" && -f "$VARS" ]] || {
      echo "error: UEFI variable store not found for $CODE; set AEGISOS_UEFI_VARS" >&2
      exit 2
    }
    vars_copy="${AEGISOS_UEFI_VARS_COPY:-build/pr1/uefi-vars.fd}"
    mkdir -p "$(dirname "$vars_copy")"
    cp "$VARS" "$vars_copy"
    firmware_mode="pflash"
    firmware_args=(
      -drive "if=pflash,format=raw,unit=0,readonly=on,file=$CODE"
      -drive "if=pflash,format=raw,unit=1,file=$vars_copy"
    )
    ;;
  *)
    firmware_args=(-bios "$CODE")
    ;;
esac

# AArch64 virt firmware and the AegisOS FDT/driver path both use the
# virtio-mmio transport.  Do not silently swap this to virtio-blk-pci: a
# firmware without the PCI virtio driver will never discover the ESP, and the
# result is an empty serial log because BOOTAA64.EFI is never launched.
BLOCK_DEVICE="${AEGISOS_UEFI_BLOCK_DEVICE:-virtio-blk-device}"
case "$BLOCK_DEVICE" in
  virtio-blk-device|virtio-blk-pci) ;;
  *)
    echo "error: unsupported AEGISOS_UEFI_BLOCK_DEVICE: $BLOCK_DEVICE" >&2
    exit 2
    ;;
esac

MACHINE="${AEGISOS_QEMU_MACHINE:-virt,secure=off,virtualization=off,gic-version=2,acpi=off}"
CPU="${AEGISOS_QEMU_CPU:-cortex-a57}"
MEMORY="${AEGISOS_QEMU_MEMORY:-8G}"

echo "[AegisOS:qemu-uefi] firmware-mode=$firmware_mode firmware=$CODE" >&2
if [[ -n "$vars_copy" ]]; then
  echo "[AegisOS:qemu-uefi] vars-copy=$vars_copy" >&2
fi
echo "[AegisOS:qemu-uefi] disk=$DISK block-device=$BLOCK_DEVICE" >&2
echo "[AegisOS:qemu-uefi] machine=$MACHINE cpu=$CPU memory=$MEMORY" >&2

exec qemu-system-aarch64 \
  -machine "$MACHINE" \
  -cpu "$CPU" \
  -smp 1 \
  -m "$MEMORY" \
  -global virtio-mmio.force-legacy=false \
  -display none \
  -serial stdio \
  -monitor none \
  -boot menu=off \
  -no-reboot \
  "${firmware_args[@]}" \
  -drive "if=none,format=raw,file=$DISK,id=disk0" \
  -device "$BLOCK_DEVICE,drive=disk0,bootindex=1" \
  -netdev user,id=net0 \
  -device virtio-net-device,netdev=net0 \
  -object rng-random,filename=/dev/urandom,id=rng0 \
  -device virtio-rng-device,rng=rng0
