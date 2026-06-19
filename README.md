# AegisOS

AegisOS is a security-focused ARM64 operating system for the AegisBox appliance family.

## Repository layout

This repository is organized around the boot chain, kernel, HAL, system services, management surfaces, image tooling, and security policy. The current implementation keeps legacy build paths such as `dashboard/`, `userland/`, and `services/` intact while a more product-oriented layout is scaffolded alongside them.

```text
AegisOS/
├── boot/                Early boot assets, linker inputs, and U-Boot files
├── kernel/              Kernel core, memory, drivers, and AArch64 arch support
├── hal/                 Board, SoC, and device-tree hardware abstractions
├── system/              Init, daemon, CLI, service, and runtime config scaffolding
├── rustmyadmin/         Dedicated admin-surface scaffold for the RustMyAdmin UI split
├── security/            Keys, signing, policies, and audit assets
├── image/               Rootfs, overlays, initramfs, and image build scripts
├── packages/            Package groups for appliance roles and features
├── targets/             Board and QEMU target descriptors
├── tools/               Build, flashing, diagnostics, update, and dev-shell helpers
└── tests/               Kernel, HAL, system, security, and integration coverage
```

## Legacy compatibility

The active build still uses the existing `Makefile`, `dashboard/`, `userland/`, `services/`, `net/`, and `fs/` paths. The additional structure in this repository provides the requested product tree without disrupting those working paths.
