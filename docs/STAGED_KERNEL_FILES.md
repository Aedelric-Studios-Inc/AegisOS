# Staged Kernel Files

These files are intentionally present but are not compiled by the active kernel Makefile yet.

## Future ARM64 architecture layer

- kernel/arch/aarch64/boot.S
- kernel/arch/aarch64/cache.S
- kernel/arch/aarch64/context_switch.S
- kernel/arch/aarch64/irq.S
- kernel/arch/aarch64/mmu.S
- kernel/arch/aarch64/syscalls.S
- kernel/arch/aarch64/vectors.S

Purpose:
These are reserved for the real ARM64 architecture layer: exception vectors, syscall entry,
MMU setup, IRQ entry, cache helpers, and context switching.

## Future filesystems

- kernel/fs/tmpfs.c
- kernel/fs/ext4.c

Purpose:
These are reserved for the real VFS/filesystem layer. They are not active while the current
rootfs is staged by the host build system.

## Future board profiles

- hal/boards/aegisbox-lite/board.c
- hal/boards/aegisbox-pro/board.c

Purpose:
These are reserved for hardware-specific board profiles once AegisBox Lite/Pro builds split.
