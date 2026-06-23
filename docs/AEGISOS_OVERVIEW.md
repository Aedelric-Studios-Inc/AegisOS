# AegisOS Overview

AegisOS is an ARM64-first operating system and router/security appliance platform for the AegisBox hardware line. It is being built as the low-level foundation for AegisBox appliances first, with a longer-term path toward PhoenixOS.

AegisOS is intentionally not Windows, macOS, Linux, or a Linux distribution. The current bring-up tree is a freestanding ARM64 kernel and appliance platform built around a layered design:

```text
ARM64 Assembly kernel entry/core paths
C hardware, memory, filesystem, network, scheduler, and syscall layer
Rust-oriented userland/security/admin direction
```

## Product purpose

The first product target is an AegisBox router/security appliance. The system is designed to support:

- secure router bring-up
- firewall/NAT/DNS/DHCP services
- VPN and tunnel services
- device/service monitoring
- RustMyAdmin integration
- appliance-grade image packaging
- future hardened userland services

## Current v2.0 status

AegisOS-v2.0 is not yet a production-ready router operating system. It is the image-packaged bring-up line after proving the kernel-to-userland spine in QEMU. V40 adds the first flash-style image layout with boot/root/config regions and QEMU disk attachment tooling.

By v38, the project had proven:

```text
EL1 kernel boot
→ scheduler/init thread
→ VFS/initramfs mounts
→ file-backed /sbin/aegis-init ELF loading
→ PT_LOAD segment copy into user backing memory
→ user/kernel permission metadata
→ argv/envp-style bootstrap
→ multiple process descriptors
→ first EL0 process launch
→ syscall return
→ process exit
→ safe post-exit scheduler handoff
```

V39 added deterministic image packaging around that proven chain. V40 adds an MBR-backed flash-style image with `AEGIS_BOOT`, `AEGIS_ROOT`, and `AEGIS_CONFIG` regions.

## Major layers

### Boot and ARM64 layer

The boot layer contains ARM64 reset entry, low-level exception/vector plumbing, EL0 transition stubs, syscall/exception entry code, and early boot coordination.

### Kernel core

The kernel core currently includes boot phases, logging, panic handling, scheduler scaffolding, timer checkpoints, process descriptors, syscall dispatch, service-manager preparation, and userland handoff state.

### Memory layer

The memory layer contains physical/virtual memory scaffolding, page table structures, heap support, and permission metadata. Hardware-backed per-process user page tables remain a later production-hardening step.

### Filesystem layer

The filesystem layer contains VFS, initramfs, configfs, and logfs scaffolding. The v36 line proved `/sbin/aegis-init` can be opened/read through VFS/initramfs and handed into the ELF loader.

### ELF loader and userland handoff

The ELF loader validates ELF64/AArch64 images, selects executable `PT_LOAD` segments, copies file bytes into a user-image backing page, zeroes the remaining memory/BSS range, and hands loaded segment metadata to the first user-process descriptor.

### Network/router layer

The tree contains early router components for Ethernet, ARP, IPv4, IPv6, ICMP, UDP, TCP, DHCP, DNS, NAT, routing, packet handling, and firewall direction. These are not yet full production network services, but the module layout is now present for router bring-up.

### RustMyAdmin and services direction

RustMyAdmin is intended as the future administrative dashboard/control plane. In v38, multiple descriptors include `aegis-init`, `aegisd`, `routerd`, and `rustmyadmin` as planned process identities.

## Honest boundary

AegisOS-v2.0/v47 packages the current kernel/userland proof into a flash-style router image artifact with persistent config metadata and adds deterministic service-supervisor fault accounting, user/kernel page-table isolation, router control-plane proof, and the AegisBox Developer Preview IMG contract. It is not yet a final hardware-flashable production OS. Remaining production work includes hardware-backed user mappings, real filesystem formatting/mount persistence, full service spawning/restart execution, storage drivers, real network I/O, secure boot/signature enforcement, installer/OTA logic, and board-specific bootloader integration.


## v43 — IPC/service messaging

V43 proves service mailbox/channel registration and a kernel-safe `SYS_SEND_MSG` / `SYS_RECV_MSG` message roundtrip between early AegisOS services.


## v44 — Service supervisor + fault/page-fault proof

V44 registers the early userland service descriptors with a kernel-side supervisor, starts them under tracked service state, records a controlled lower-EL page-fault-class event, marks `routerd` faulted, and proves the kernel handoff boundary remains intact. Full hardware-backed user/kernel page-table isolation remains the v45 target.

Expected proof line:

```text
QEMU v44 proof accepted: service supervisor registered early services, recorded controlled page-fault state, marked routerd faulted, and preserved the EL0 launch/exit chain.
```


## v46 — Network control-plane proof

V46 proves the router control plane: deterministic network stack bring-up, synthetic DHCP lease binding, IPv4/default route configuration, default-deny firewall policy, NAT enablement, and preservation of the v45 isolation chain.

## v47 — Developer Preview image

V47 packages the proved chain into the AegisBox Developer Preview IMG contract with Lite, Pro, Bastion, and Router variants. It is a Developer Preview milestone, not the final v55 Release IMG.

## v48 — Release polish and service config matrix

V48 preserves the v47 Developer Preview IMG contract and adds a release-polish
proof layer: board profile metadata, service preset defaults, installer/flash
validation gates, documentation gates, and a security-hardening baseline. This
starts the v48-v54 polish run toward the v55 Release IMG.
