# Production Prep Notes

This ZIP is the first hardening pass over the scaffold repository.

## Completed in this pass

- Removed committed Rust `target/` build artefacts.
- Added a root `.gitignore` for build outputs, secrets, and production keys.
- Fixed the invalid AArch64 literal in `boot/arm64/mmu.S`.
- Moved the public `struct vfs` definition into `fs/include/vfs.h`.
- Removed the duplicate `sys_cap_grant` symbol by renaming the IPC helper to `cap_delegate`.
- Fixed the broken timer include path and added a kernel timer header.
- Added a process header and wired early process initialisation into `kernel_main`.
- Added idle and `aegisbox-init` tasks so the scheduler is no longer launched empty.
- Changed the early kernel heap to a low bring-up address until higher-half/MMU mapping is complete.
- Made RustMyAdmin auth fail closed unless a token is configured or explicit dev-loopback mode is enabled.
- Replaced pseudo-random RustMyAdmin session/CSRF tokens with `/dev/urandom`-backed tokens.
- Fixed CSRF validation so it checks the actual token value.
- Made `aegisd` fail closed unless `AEGISD_TOKEN` or explicit dev-local mode is configured.
- Added `aegisd` `/radio/status` telemetry for the LTE/SIM router path.
- Added AegisBox and PhoenixOS edition manifests to separate product constraints from the reusable core.

## Still not production ready

- The custom kernel still needs a real MMU/page-table bring-up path.
- The physical allocator still needs full reserved-region handling for kernel, DTB, initramfs, stacks, and page tables.
- Rust userland currently assumes Linux/POSIX services and is not yet runnable on the custom kernel.
- The dashboard/RustMyAdmin surfaces still need deeper live `aegisd` integration.
- The image builder needs reproducible signing, recovery, and rollback enforcement.
- The LTE/radio path still needs real modem integration testing on hardware.
