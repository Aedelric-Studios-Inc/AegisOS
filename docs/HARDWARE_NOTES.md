# AegisOS v55 Hardware Notes

Target family:

- AegisBox Lite
- AegisBox Pro
- AegisBox Bastion
- ÆÐELRIC Router / AegisBox Router profile

Current proof target:

- ARM64/AArch64
- QEMU `virt` machine
- Cortex-A57 CPU model in the smoke/proof scripts
- Raw flash-style image attached as a virtio block device

Recommended developer hardware direction:

- Serial console available before hardware flash testing.
- Recovery card or known-good image kept offline.
- Avoid flashing production/personal storage devices without checking `lsblk` and the release manifest first.
- Router/Bastion profiles should have at least two network interfaces for eventual WAN/LAN testing.
