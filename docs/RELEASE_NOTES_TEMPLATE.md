# AegisOS v2.0 Release Notes Template

## Release name

AegisOS v2.0 `<milestone>`

## Status

- Developer Preview
- Release Candidate
- Final Release IMG

## Supported targets

- AegisBox Lite
- AegisBox Pro
- AegisBox Bastion
- ÆÐELRIC Router
- QEMU AArch64 virt

## Proven milestones

```text
v39 packageable image
v40 flash layout + QEMU disk/image boot + persistent config
v41 block/storage abstraction + process table cleanup
v42 real multiprocess launch + syscall expansion
v43 IPC/service messaging
v44 service supervisor + fault/page-fault proof
v45 real user/kernel page-table isolation
v46 network bring-up + DHCP + NAT/firewall control plane
v47 AegisBox Developer Preview IMG
v48 release polish gates
```

## What works

- ARM64 kernel boot path
- Init thread and VFS/initramfs bootstrap
- File-backed `/sbin/aegis-init` validation and EL0 launch/exit proof
- Multiprocess descriptor preparation
- Syscall expansion proof
- IPC/service mailbox roundtrip proof
- Service supervisor controlled fault proof
- TTBR0 user/kernel isolation proof
- Synthetic router network control-plane proof
- Developer Preview IMG wrapper and release-polish manifests

## Known limitations

- The image is still booted in QEMU with `-kernel build/aegisos.elf` plus attached raw disk image.
- A standalone firmware/UEFI boot path is not yet promoted.
- v46 DHCP/NAT/firewall is a control-plane proof, not yet a real packet-forwarding hardware validation.
- Interactive shell/login is not yet the main userland flow.
- Hardware flashing must remain Developer Preview until board-specific boot firmware and recovery are validated.

## Upgrade / install notes

Use the release manifest and SHA256 files to verify artifacts before flashing or VM boot.

