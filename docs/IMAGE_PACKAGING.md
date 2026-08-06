# AegisOS v2.0 Image Packaging

AegisOS-v2.0 image packaging has two stages so far:

```text
v39: packageable deterministic raw image container
v40: flash-style raw disk image with partition table and config partition
```

## v39 packageable image

V39 introduced the first deterministic AegisOS router image package:

```text
build/images/AegisOS-v2.0-router-aarch64.img
```

The v39 image contains an AegisOS header, the built ARM64 kernel payload, and metadata blocks. It intentionally avoids root-required tooling.

## v40 flash-layout image

V40 introduces the first flash-style image:

```text
build/images/AegisOS-v2.0-router-aarch64-v40-flash.img
```

It contains:

```text
MBR partition table
AEGIS_BOOT partition region
AEGIS_ROOT partition region
AEGIS_CONFIG persistent config region
```

The v40 image builder emits:

```text
build/images/AegisOS-v2.0-router-aarch64-v40-flash.img
build/images/AegisOS-v2.0-router-aarch64-v40-flash.img.manifest.json
build/images/AegisOS-v2.0-router-aarch64-v40-flash.img.sha256
build/images/AegisOS-v2.0-router-aarch64-v40-flash.img.config.json
```

## Build command

```bash
tools/dev/fix-clock-skew.sh
tools/qemu/build-and-smoke-aarch64.sh
```

## Manual image build

```bash
AEGISOS_QEMU_SMOKE=1 AEGISOS_FORCE_REBUILD=1 tools/build/kernel-clang-aarch64.sh
tools/image/build-aegisos-v2-flash-img.sh
```

## Inspection

```bash
tools/image/inspect-aegisos-v2-flash-img.py build/images/AegisOS-v2.0-router-aarch64-v40-flash.img
```

## Flashing boundary

V40 is not yet the final hardware-flash production image. It establishes the real image layout and persistent config partition, but filesystem formatting, bootloader loading from the boot region, and kernel-side block-device reads are later milestones.
