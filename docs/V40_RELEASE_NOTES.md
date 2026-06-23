# AegisOS v2.0 v40 Release Notes

## Milestone

```text
AegisOS-v2.0-v40-flash-layout-disk-config
```

## Added

- Flash-style raw disk image generation.
- MBR partition table generation.
- `AEGIS_BOOT`, `AEGIS_ROOT`, and `AEGIS_CONFIG` regions.
- Persistent config JSON payload.
- Image manifest, SHA-256 checksum, and extracted config artifact.
- QEMU disk/image boot path with the v40 image attached as a virtio block device.
- Image inspection tool for validating the MBR and partition headers.

## Preserved

- v35 ELF validation/bind proof.
- v36 VFS/initramfs file-backed ELF proof.
- v37 PT_LOAD copy and permission proof.
- v38 argv/envp and multiple descriptor proof.
- v39 packageable image direction.

## Expected proof lines

```text
AegisOS v2.0/v40 flash image layout accepted: build/images/AegisOS-v2.0-router-aarch64-v40-flash.img
QEMU v40 disk/image boot path accepted: kernel booted with flash image attached: build/images/AegisOS-v2.0-router-aarch64-v40-flash.img
```

## Boundary

V40 is the flash-layout/disk-attachment milestone. It does not yet implement kernel block-driver reads from the flash image or firmware bootloader loading from the boot partition.
