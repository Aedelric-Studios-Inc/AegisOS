# AegisOS v2.0

AegisOS is an ARM64-first operating system and router/security appliance platform for the AegisBox hardware family and the ÆÐELRIC Router line. It is built as a freestanding OS direction, not as a Linux distribution, with a layered architecture built around:

```text
ARM64 Assembly kernel entry/core paths
C hardware, memory, filesystem, scheduler, syscall, and network layer
Rust-oriented security/userland/admin direction
```

## Current milestone

This tree is:

```text
AegisOS-v2.0-v55.1-interactive-runtime
```

V48 preserves the v47 Developer Preview IMG contract and adds release-polish proof gates for board profiles, service defaults, installer/flash validation, documentation, and security-hardening readiness across Lite, Pro, Bastion, and Router variants.

## What is proved before packaging

The v2.0 line carries forward the v27–v38 QEMU-proven boot/userland chain:

```text
EL1 kernel boot
→ scheduler/init thread
→ VFS/initramfs mounts
→ /sbin/aegis-init opened/read through VFS
→ ELF64/AArch64 validation
→ PT_LOAD copy into user image backing
→ BSS/tail zeroing
→ user/kernel permission metadata
→ argv/envp-style bootstrap metadata
→ multiple process descriptors
→ first EL0 process launch
→ syscall return path
→ user process exit
→ safe post-exit scheduler handoff
```

V39 proved deterministic image packaging. V40 adds the next image boundary:

```text
MBR flash-style image
→ AEGIS_BOOT partition region
→ AEGIS_ROOT partition region
→ AEGIS_CONFIG persistent config region
→ QEMU virtio block attachment path
→ image inspection/checksum artifacts
```

## Build, prove, and package

Use the QEMU proof harness. It builds the ARM64 kernel, proves the boot/userland milestones, builds the v40 flash-layout image, then proves the image can be attached to QEMU as the future block device while the known-good kernel path boots.

```bash
tools/dev/fix-clock-skew.sh
tools/qemu/build-and-smoke-aarch64.sh
```

Expected final v48 lines:

```text
QEMU v46 proof accepted: network stack brought up, synthetic DHCP lease bound, NAT/firewall control plane loaded, and v45 isolation chain preserved.
QEMU v47 proof accepted: AegisBox Developer Preview IMG contract prepared with Lite/Pro/Bastion/Router variants and EL0 launch/exit chain preserved.
QEMU v48 proof accepted: release polish gates, board profile matrix, service config presets, security hardening baseline, and docs/manifest checks prepared while preserving the v47 Developer Preview chain.
AegisOS v2.0/v48 release polish manifest accepted: build/images/release-polish/AegisOS-v2.0-v48-release-polish-manifest.json
QEMU v40 disk/image boot path accepted: kernel booted with flash image attached: build/images/AegisOS-v2.0-router-aarch64-v40-flash.img
```



## v55.1 interactive Release VM

The release VM now has a normal non-smoke serial console. Use this helper instead of the smoke proof harness when you want a prompt:

```bash
tools/qemu/boot-release-vm-aarch64.sh
```

Expected prompt:

```text
AegisOS v2.0 Release — v55.1 interactive runtime
aegis:/#
```

Useful first commands: `help`, `status`, `services`, `rust`, `rustmyadmin`, `net`, `rootfs`, `ls /etc`, `cat /etc/services.toml`.

Native Rust process execution is still the next runtime step; v55.1 connects/catalogues the Rust userland, services, RustMyAdmin, dashboard, and security source trees into the shell and rootfs bridge.

## Release-candidate preparation

This tree includes a v49 release-candidate preparation layer on top of the v48 release-polish proof. It adds release checklists, release notes templates, artifact verification, a release-candidate manifest generator, and a QEMU Developer Preview VM boot helper.

```bash
tools/release/prepare-release-candidate.sh
tools/release/verify-release-artifacts.sh
tools/qemu/boot-dev-preview-vm-aarch64.sh
```

This tree includes the v55 Release IMG layer. The formal lock-in still requires the QEMU proof and release artifact verification to pass on the release host.

## Image output

After a successful v40 run, the image artifacts are:

```text
build/images/AegisOS-v2.0-router-aarch64-v40-flash.img
build/images/AegisOS-v2.0-router-aarch64-v40-flash.img.manifest.json
build/images/AegisOS-v2.0-router-aarch64-v40-flash.img.sha256
build/images/AegisOS-v2.0-router-aarch64-v40-flash.img.config.json
```

The v40 `.img` is a flash-style raw disk image with an MBR table and three AegisOS regions:

```text
AEGIS_BOOT    boot/kernel payload region
AEGIS_ROOT    reserved root/userland region
AEGIS_CONFIG  persistent configuration region
```

The current QEMU boot path still uses `-kernel build/aegisos.bin` because no firmware bootloader has been added yet. The v40 image is attached as a virtio block device to establish the disk/image boot path for v41+ storage work.

## Manual QEMU disk-image boot path

```bash
tools/image/build-aegisos-v2-flash-img.sh
tools/qemu/boot-v40-disk-image-aarch64.sh build/images/AegisOS-v2.0-router-aarch64-v40-flash.img
```

Stop QEMU with `Ctrl+A`, then `X`.

## Documentation

- `docs/AEGISOS_OVERVIEW.md` — what AegisOS is, its layers, and current v2.0 boundary.
- `docs/MILESTONE_HISTORY.md` — consolidated milestone history through v40.
- `docs/IMAGE_PACKAGING.md` — how image generation evolved from v39 to v40.
- `docs/FLASH_IMAGE_LAYOUT.md` — v40 partition/region layout.
- `docs/QEMU_DISK_BOOT.md` — v40 QEMU disk-image attachment path.
- `docs/PERSISTENT_CONFIG.md` — AEGIS_CONFIG persistent configuration region.
- `docs/V40_RELEASE_NOTES.md` — release notes for the v40 milestone.

## Current boundary

AegisOS-v2.0 v40 now has a flash-style image layout and persistent config partition metadata. Remaining production work includes real block-driver reads from the attached disk, true filesystem formatting inside partitions, firmware/bootloader loading from the boot partition, real board flash instructions, hardware-backed user page tables, actual service spawning, network-driver I/O, OTA/update flow, and final AegisBox release image signing.

## v41 milestone

AegisOS v41 adds the kernel-facing storage/process foundation on top of the v40 flash image.

- Basic block/storage abstraction for `aegis-flash0`
- Registered `AEGIS_BOOT`, `AEGIS_ROOT`, and persistent `AEGIS_CONFIG` regions
- Process table cleanup and invariant proof
- QEMU proof markers for storage abstraction and process table cleanup

Expected v41 proof line:

```text
QEMU v41 proof accepted: block/storage abstraction registered the v40 flash layout and process table cleanup invariants held.
```


## v42 milestone

AegisOS v42 adds the real multi-process launch path and syscall expansion on top of the v41 storage/process foundation. The kernel now proves that the `aegis-init`, `aegisd`, `routerd`, and `rustmyadmin` descriptors are launchable, stack-bootstrapped, and reachable through a safer process/service syscall surface.


## v43 milestone

AegisOS v43 adds IPC/service messaging on top of the v42 multi-process/syscall foundation. The kernel now proves that early services have mailbox channels and that the expanded `SYS_SEND_MSG` / `SYS_RECV_MSG` path can complete a deterministic message roundtrip.

Expected v43 proof line:

```text
QEMU v43 proof accepted: IPC/service messaging mailboxes prepared and kernel-safe message roundtrip proved.
```

## v44 milestone

AegisOS v44 adds the first service supervisor and controlled fault/page-fault proof on top of the v43 IPC service messaging layer. The kernel registers the early userland service descriptors, starts them under supervisor state, records a controlled lower-EL page-fault-class event against `routerd`, marks that service faulted, and preserves the EL0 launch/exit proof chain.

Expected v44 proof line:

```text
QEMU v44 proof accepted: service supervisor registered early services, recorded controlled page-fault state, marked routerd faulted, and preserved the EL0 launch/exit chain.
```


## v43.1 IPC Roundtrip Patch

v43.1 is a patch release for the v43 IPC/service messaging milestone. It keeps
the same milestone scope but fixes the QEMU proof path so the IPC roundtrip is
bounded and deterministic before the first EL0 user process launch proof.

Expected proof line:

```text
QEMU v43.1 proof accepted: IPC/service messaging mailboxes prepared, bounded SYS_SEND_MSG/SYS_RECV_MSG roundtrip proved, and EL0 launch/exit chain preserved.
```


## v43.2 IPC Roundtrip Patch

AegisOS v43.2 fixes the IPC proof path by resetting channel state and proving both direct syscall-handler and syscall-dispatch SYS_SEND_MSG/SYS_RECV_MSG roundtrips before continuing to the existing EL0 launch/exit chain.


## v43.3 IPC Roundtrip Patch

AegisOS v43.3 fixes the remaining IPC proof gap by proving the kernel-owned service mailbox channel directly through the IPC queue backing. v43.1 and v43.2 reached `service_ipc_message_roundtrip_selftest()` but did not reach `qemu_boot_checkpoint_ipc_message_roundtrip_ready`. v43.3 makes the roundtrip deterministic: enqueue, dequeue, byte-compare, mark mailbox proved, and preserve the EL0 launch/exit chain.

Target proof:

```text
QEMU v43.3 proof accepted: IPC/service messaging mailboxes prepared, kernel-owned mailbox roundtrip proved, and EL0 launch/exit chain preserved.
```


## v43.4 IPC Roundtrip Diagnostic Patch

AegisOS v43.4 fixes the remaining IPC roundtrip timeout by moving the QEMU proof to a strictly bounded kernel-owned mailbox buffer. It adds micro-checkpoints for roundtrip entry, mailbox selection, send, receive, compare, and final ready state.

```text
QEMU v43.4 proof accepted: IPC/service messaging mailboxes prepared, diagnostic direct-mailbox roundtrip proved, and EL0 launch/exit chain preserved.
```


## v45 milestone

AegisOS v45 adds real user/kernel page-table isolation proof on top of the v44 service supervisor layer. The kernel now prepares per-process TTBR0 page-table roots, maps bounded EL0 user regions with real PTE permissions, leaves guard pages unmapped, verifies the kernel window is blocked from user roots, and preserves the EL0 launch/exit proof chain.

Expected v45 proof line:

```text
QEMU v45 proof accepted: per-process TTBR0 roots created, EL0 user regions mapped, kernel window blocked, guard pages unmapped, and EL0 launch/exit chain preserved.
```


## v46 milestone

AegisOS v46 adds network bring-up plus DHCP/NAT/firewall control-plane proof.

```text
QEMU v46 proof accepted: network stack brought up, synthetic DHCP lease bound, NAT/firewall control plane loaded, and v45 isolation chain preserved.
```

## v47 milestone

AegisOS v47 is the AegisBox Developer Preview IMG milestone. It prepares Lite, Pro, Bastion, and Router image variant contracts and emits Developer Preview image metadata after the QEMU proof chain passes.

```text
QEMU v47 proof accepted: AegisBox Developer Preview IMG contract prepared with Lite/Pro/Bastion/Router variants and EL0 launch/exit chain preserved.
```


## v48 milestone

AegisOS v48 adds the first polish layer after the Developer Preview IMG:

- board profile matrix for Lite, Pro, Bastion, and Router
- service config presets for the early appliance services
- installer/flash validation gates
- security-hardening and documentation readiness markers
- release-polish manifest generation under `build/images/release-polish/`

Expected v48 proof line:

```text
QEMU v48 proof accepted: release polish gates, board profile matrix, service config presets, security hardening baseline, and docs/manifest checks prepared while preserving the v47 Developer Preview chain.
```

## v50-v55 Release IMG promotion

AegisOS v55 promotes the v48/v49 release-candidate work through the final release gates:

```text
v50 installer/flash flow hardening
v51 Lite/Pro/Bastion/Router variant images
v52 security hardening + service defaults freeze
v53 docs + known limitations + hardware notes
v54 final RC audit/signing/checksums
v55 Release IMG
```

Run:

```bash
tools/dev/fix-clock-skew.sh
tools/qemu/build-and-smoke-aarch64.sh
tools/release/verify-v55-release.sh
```

Expected final lines include:

```text
QEMU v55 proof accepted: Release IMG contract prepared while preserving v46 network, v45 isolation, v44 supervisor, and EL0 launch/exit chain.
AegisOS v2.0/v55 Release IMG accepted: build/images/release-v55/AegisOS-v2.0-v55-release-aarch64.img
```
