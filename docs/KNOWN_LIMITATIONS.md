# AegisOS v55 Known Limitations

AegisOS v55 is a Release IMG milestone for the current AegisBox/AArch64 proof chain, not a finished general-purpose OS distribution.

Known limitations:

- QEMU boot still uses `-kernel build/aegisos.elf` or `build/aegisos.bin` with the raw image attached as a block device.
- The raw `.img` is not yet a fully standalone UEFI/firmware-bootable image.
- v46 DHCP/NAT/firewall is a deterministic control-plane proof; real packet I/O and production network-driver backends remain future work.
- Userland is still a controlled tiny ELF launch/exit proof path, not an interactive shell/login environment.
- Cryptographic release signing is represented by a signing-manifest placeholder and SHA-256 sidecars until production keys are provisioned.
- Hardware flashing must be treated as developer/owner testing until board-specific firmware, storage, and recovery procedures are validated on real devices.
