# AegisOS Boot Flow

## Sequence

1. U-Boot loads the AegisOS image.
2. `boot/start.S` hands control to the ARM64 startup path.
3. `boot/uboot/` assets provide the bootloader integration entry points.
4. The AArch64 vector, cache, MMU, and syscall stubs are staged under `kernel/arch/aarch64/`.
5. Kernel core initialization proceeds through memory setup, scheduling, IPC, drivers, filesystems, and system startup.
6. The runtime pivots into `system/init/` and the service layer.

## Compatibility note

The active implementation still keeps the original boot assembly under `boot/arm64/` and the detailed boot process description in `docs/boot-process.md`.
