# AegisOS v40 Persistent Config Partition

V40 introduces `AEGIS_CONFIG`, a dedicated persistent configuration region inside the flash-style image.

## Purpose

The config partition is where AegisOS will store board profile, router mode, service enablement, network defaults, and admin handoff settings.

## Current format

The current v40 payload is JSON with a checksum recorded in the image manifest.

Current profile set:

```text
aegisbox-lite
aegisbox-pro
aegisbox-bastion
aedelric-router
```

Current planned service domains:

```text
aegis-init
aegisd
routerd
dhcpd
dnsd
firewalld
rustmyadmin
```

## Boundary

The config payload is written into the image and emitted separately as:

```text
build/images/AegisOS-v2.0-router-aarch64-v40-flash.img.config.json
```

Kernel-side mounting/parsing of the persistent config partition is a later milestone after v41 block/storage abstraction.
