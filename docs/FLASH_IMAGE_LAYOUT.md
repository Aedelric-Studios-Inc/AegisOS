# AegisOS v40 Flash Image Layout

V40 converts the v39 packageable raw image container into a flash-style disk image.

The image is still developer-oriented, but it now has a concrete partition table and stable regions that can become real filesystems later.

## Image type

```text
format: raw disk image
partition table: MBR
sector size: 512 bytes
default size: 256 MiB
```

## Partitions

```text
partition 1: AEGIS_BOOT
  type: 0x0C
  role: boot/kernel payload region
  start: 1 MiB aligned
  size: 32 MiB

partition 2: AEGIS_ROOT
  type: 0x83
  role: future root/userland region
  size: 160 MiB

partition 3: AEGIS_CONFIG
  type: 0xDA
  role: persistent configuration region
  size: 32 MiB
```

## Current contents

`AEGIS_BOOT` contains an AegisOS partition header and the built ARM64 kernel payload.

`AEGIS_ROOT` contains an AegisOS root-region header and the image manifest. The real service/userland filesystem is a later milestone because `/sbin/aegis-init` is still supplied by the proven builtin initramfs path.

`AEGIS_CONFIG` contains an AegisOS persistent configuration header and a JSON configuration payload for board/profile/service defaults.

## Boundary

This is not yet the final production-flash image. It is the first real flash-style layout that QEMU can attach as a disk while the proven kernel path boots with `-kernel`.
