# AegisOS v40 QEMU Disk/Image Boot Path

V40 adds the QEMU disk/image boot path around the proven kernel boot path.

## Current boot method

The current reliable ARM64 QEMU path remains:

```text
-kernel build/aegisos.bin
```

V40 adds the generated flash image as a virtio block device:

```text
-drive if=none,file=build/images/AegisOS-v2.0-router-aarch64-v40-flash.img,format=raw,id=aegisflash
-device virtio-blk-device,drive=aegisflash
```

This proves the image can be generated, inspected, attached to QEMU, and used as the future storage target without breaking the existing boot path.

## Manual run

```bash
AEGISOS_QEMU_SMOKE=1 AEGISOS_FORCE_REBUILD=1 tools/build/kernel-clang-aarch64.sh
tools/image/build-aegisos-v2-flash-img.sh
tools/qemu/boot-v40-disk-image-aarch64.sh build/images/AegisOS-v2.0-router-aarch64-v40-flash.img
```

## Proof run

```bash
tools/qemu/prove-v40-disk-image-aarch64.sh \
  build/images/AegisOS-v2.0-router-aarch64-v40-flash.img \
  build/aegisos.bin \
  build/aegisos.elf
```

Expected line:

```text
QEMU v40 disk/image boot path accepted: kernel booted with flash image attached: build/images/AegisOS-v2.0-router-aarch64-v40-flash.img
```

## Boundary

This is not a firmware disk boot yet. Firmware/bootloader loading directly from `AEGIS_BOOT` comes later. V40 establishes the disk-image path required for the v41 block/storage abstraction work.
