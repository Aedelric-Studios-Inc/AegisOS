# AegisOS Release Readiness Checklist

This checklist describes the remaining work after the v57 native supervisor
chain. A build artifact or catalogue entry does not count as complete: each
item needs guest-side or hardware-side runtime evidence.

## 1. Native userspace and process lifecycle

- [ ] v57 PID 1 -> service-manager -> aegisd QEMU proof passes repeatedly.
- [ ] Replace the assembly bootstrap service-manager and aegisd with their
      production native AegisOS implementations.
- [ ] Implement blocking wait/reap, process image cleanup, runtime-slot release,
      restart policy, backoff, and clean shutdown.
- [ ] PID 1 detects and restarts a failed service-manager.
- [ ] Service-manager detects and restarts a failed aegisd.
- [ ] Implement exception-frame-aware preemption, or formally retain and prove
      a cooperative scheduling model for every service.
- [ ] Long-running scheduler/process/IPC soak tests show no deadlocks or leaks.

## 2. Native Rust platform

- [ ] Define and version an `aarch64-unknown-aegisos` Rust target.
- [ ] Provide AegisOS startup, panic, allocator, syscall, filesystem, IPC,
      threading, time, and networking support.
- [ ] Port the required Rust `core`/`alloc` and either `std` or a documented
      `no_std` service framework.
- [ ] Compile and execute the actual Rust service-manager and aegisd inside the
      AegisOS guest, with no `aarch64-unknown-linux-*` dependency.
- [ ] Port every production Rust service in dependency order and prove its
      native health endpoint.

## 3. IPC, capabilities, and isolation

- [ ] Add channel ownership/ACLs, capability checks, namespaces, close/delete,
      blocking receive or poll, and bounded resource accounting.
- [ ] Complete per-process address spaces and enforce RX/RW/NX permissions in
      hardware rather than metadata only.
- [ ] Add user exception delivery/termination and supervisor fault reporting.
- [ ] Fuzz malformed syscalls, ELF files, IPC messages, and service manifests.

## 4. Boot and hardware discovery

- [ ] Pass a valid FDT to the kernel; current QEMU boot still reports `dtb=0x0`.
- [ ] Discover UART, GIC, virtio, PCIe, block and network devices from firmware
      data rather than hard-coded QEMU assumptions.
- [ ] Boot from the attached virtual disk through UEFI or U-Boot instead of
      relying on QEMU `-kernel`.
- [ ] Implement signed boot assets and a tested recovery boot path.

## 5. Storage and installation

- [ ] Real filesystem mounts and persistent read/write configuration.
- [ ] Virtio-block persistence and power-loss tests in QEMU.
- [ ] PCIe/NVMe driver and installer support for AegisBox Pro and Bastion.
- [ ] USB installer -> NVMe partition/install/reboot/recovery workflow.
- [ ] Encryption, update staging, rollback, backup and filesystem-repair paths.
- [ ] Confirm the Router's final storage hardware separately.

## 6. Native networking and router functionality

- [ ] Virtio-net RX/TX driver with interrupt and queue handling.
- [ ] Physical NIC drivers for each release board.
- [ ] Real Ethernet, ARP, IPv4/IPv6, ICMP, UDP and TCP packet I/O.
- [ ] DHCP client/server, DNS, routing, NAT/conntrack and default-deny firewall
      operate inside AegisOS, not through the Arch host kernel.
- [ ] Router VM uses separate WAN and LAN NICs with a client VM proving DHCP,
      DNS, NAT, firewall and failure behaviour end to end.
- [ ] Native VPN, traffic monitoring, intrusion monitoring and device discovery.

## 7. Dashboard and RustMyAdmin

- [ ] Native socket ABI: bind/listen/accept/connect/send/receive/poll/timeouts.
- [ ] Native TLS/crypto, entropy and trustworthy time source.
- [ ] Dashboard listens inside AegisOS and is reachable through a guest NIC.
- [ ] RustMyAdmin runs as a native supervised process with persistent storage,
      authentication, least privilege, audit logs and backup/restore tests.

## 8. Product profiles

- [ ] Remove the current `BOARD_BASTION` versus Router-labelled image mismatch.
- [ ] Separate Pro, Bastion and Router board definitions, service manifests,
      firewall policies, storage layouts and release names.
- [ ] Pro and Bastion install to NVMe; neither uses microSD as primary storage.
- [ ] Validate each profile from clean build through installed-device boot.

## 9. Release engineering and security

- [ ] Reproducible clean builds, checksums, real signing keys and key rotation.
- [ ] SBOM, third-party licence audit and source/provenance records.
- [ ] No development secrets, test credentials or host-specific paths in images.
- [ ] Secure update, rollback protection and recovery-media verification.
- [ ] Threat model, code review, static analysis, fuzzing and penetration tests.

## 10. Release validation

- [ ] Automated QEMU build, boot, install, upgrade, rollback and fault tests.
- [ ] Hardware matrix for every supported Pro, Bastion and Router board/NVMe/NIC.
- [ ] Thermal, power, reboot, watchdog and extended soak testing.
- [ ] Factory-reset and disaster-recovery tests.
- [ ] Versioned documentation, installer instructions and known limitations.
- [ ] Release candidate freeze followed by a final independent security audit.

## Current release boundary

After v57 passes, AegisOS has a real native three-process bootstrap and IPC
chain. It is still not release-ready because production Rust services, native
network packet I/O, persistent filesystems, NVMe installation, standalone boot,
and product-profile separation remain unproved.
