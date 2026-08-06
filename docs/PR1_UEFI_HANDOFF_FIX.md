# AegisOS PR1 UEFI handoff correction

The previous UEFI proof could time out with an empty serial log because the path had four concrete weaknesses:

1. The UEFI application emitted no direct PL011 diagnostics, so firmware, filesystem, ELF-load and handoff failures were indistinguishable.
2. The loader read the complete kernel into an EFI pool allocation before reserving the kernel's fixed physical load range. That ordering allowed EFI pool allocation to occupy pages later requested with `AllocateAddress`.
3. The same low `0x40080000` link address used by QEMU `-kernel` was reused under firmware, where low guest RAM is also used by EDK2.
4. The loader jumped to the kernel without normalising the AArch64 exception level, MMU/cache state and timer access expected by the bare-metal entry path.

This correction:

- streams ELF program segments directly from the EFI file after reserving one contiguous target range;
- builds a dedicated UEFI kernel at `0x50080000`, while keeping the ordinary `-kernel` image at `0x40080000`;
- adds an AArch64 EL2/EL1 handoff routine that masks interrupts, cleans the loaded image, disables firmware MMU/cache state, enables EL1 timer access and enters EL1h;
- emits direct UART stage and failure-status lines from the UEFI application;
- uses a PCI virtio boot disk for wider EDK2 compatibility and handles split CODE/VARS firmware images correctly;
- writes a standards-complete FAT32 hidden-sector value and backup FSInfo sector;
- makes the proof require both UEFI-stage lines and actual AegisOS kernel/FDT lines.

The direct QEMU kernel build remains unchanged at entry `0x40080000`. The dedicated UEFI kernel build has entry `0x50080000`.
