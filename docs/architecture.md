# AegisOS Architecture

## Overview

AegisOS is a security-focused operating system designed for the AegisBox family of hardware appliances.
It is built on ARM64 (AArch64) and written in a combination of ARM64 assembly, C, and Rust.

## Layers

### Boot Layer (`boot/`)
ARM64 assembly entry points, exception vector tables, MMU initialization, and U-Boot integration.

### Hardware Abstraction Layer (`hal/`)
Board-specific initialization, GIC interrupt controller, ARM generic timer, device tree parsing,
and drivers for UART, GPIO, Ethernet, storage, USB, and watchdog.

### Kernel (`kernel/`)
Microkernel core: memory management (physical allocator, virtual memory, page tables, heap),
scheduler, IPC (message passing, channels, capabilities), system call dispatch, and panic handling.

### Networking (`net/`)
Full network stack: Ethernet, ARP, IPv4, IPv6, ICMP, UDP, TCP, DHCP, DNS, NAT, routing, and firewall.

### Filesystem (`fs/`)
Virtual filesystem layer, initramfs, configuration filesystem, and log filesystem.

### Security Layer (`security/`)
Rust-based security subsystem: capabilities, sandboxing, audit logging, keyring, integrity checking,
secure boot verification, and policy engine.

### Userland (`userland/`)
Rust userland: init, aegisctl management tool, shell, service manager, netctl, update manager,
recovery, and diagnostics.

### Services (`services/`)
Rust system services: firewall, VPN, DNS filter, traffic monitor, intrusion watch, device discovery,
Cloudflare tunnel, web hosting, and container runner.

### Dashboard (`dashboard/`)
Rust-based web dashboard for device management.

## Target Hardware

- AegisBox Lite
- AegisBox Pro
- Bastion
