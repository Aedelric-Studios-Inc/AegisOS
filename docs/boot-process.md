# AegisOS Boot Process

## Sequence

1. **U-Boot** loads the AegisOS kernel image from storage.
2. `boot/arm64/start.S` — reset handler, CPU mode setup, stack initialization.
3. `boot/arm64/mmu.S` — early MMU and cache configuration.
4. `boot/arm64/vectors.S` — install exception vector table.
5. `boot/arm64/cache.S` / `boot/arm64/tlb.S` — flush caches and TLBs.
6. Jump to `kernel_main()` in `kernel/core/kernel_main.c`.
7. Kernel initializes physical memory, virtual memory, heap, scheduler, IPC, and HAL.
8. HAL probes device tree and initializes board peripherals.
9. Network stack, filesystems, and security subsystem are initialized.
10. `init` process is launched from userland.

## Secure Boot

Secure boot verification is performed by `security/src/secure_boot.rs` before executing any
untrusted image. The bootloader validates signatures using keys stored in the keyring.

## U-Boot Integration

See `boot/uboot/boot.cmd` and `boot/uboot/extlinux.conf` for U-Boot configuration.
