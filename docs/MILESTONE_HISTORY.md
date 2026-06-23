# AegisOS v2.0 Milestone History

This document consolidates the v30–v38 bring-up notes that previously lived as separate root README files. The v39/AegisOS-v2.0 tree keeps the root README focused and moves milestone history into this document.

# AegisOS Router Bring-up v30

Adds the pre-EL0 userland contract proof:

- fake first user process descriptor for `/sbin/aegis-init`
- user virtual address layout proof
- user stack + guard page proof
- kernel-safe syscall ABI proof
- QEMU proof checkpoints for all of the above

EL0 transition and real user process launch are intentionally deferred to v31/v32.

---

# AegisOS Router Bring-up v31.0 — Controlled EL0 Transition

This build keeps the userland-launch milestone inside the v31 line. Bugfixes
should be named v31.1, v31.2, etc. until the controlled EL0 transition is solid.

Expected QEMU proof line:

```text
QEMU v31.0 proof accepted: controlled EL0 eret transition entered EL0 stub and trapped back through SVC.
```

---

# AegisOS Router Bring-up v32 — First Tiny User Process

Milestone: promote the pre-EL0 `aegis-init` descriptor into the first tiny user process and prove it reaches EL0.

Proof target:

```text
EL1 kernel
→ first aegis-init process descriptor launch-ready
→ eret into tiny EL0 user body
→ SYS_GETPID SVC trap
→ EL1 el0_sync_a64 vector
→ userland_mark_first_user_process_syscall
```

This is not the full ELF loader yet. ELF loading, return-to-user continuation, full process exit handling, and isolated user page tables remain future milestones.

---

# AegisOS Router Bring-up v33 — Syscall Return Path

v33 proves the first tiny `aegis-init` user process can make a syscall, receive
its return value in `x0`, resume in EL0 after `svc`, and deliberately trap again
so the kernel can prove continuation.

Scope:
- retain v27–v32 boot/userland proofs
- dispatch `SYS_GETPID` from the first tiny EL0 process
- write the syscall return value into the saved EL0 `x0` slot
- return through `restore_regs` + `eret`
- reach `aegis_first_user_process_after_syscall`
- trigger a second proof SVC from `aegis_first_user_process_return_probe`
- mark continuation in kernel state

Deferred:
- process exit
- real ELF loading
- isolated user page tables
- scheduler-owned user process lifecycle

---

# AegisOS Router Bring-up v34 — Tiny User Process Exit

This milestone builds on v33.

V33 proved the syscall return path: the first tiny `aegis-init` EL0 body called `SYS_GETPID`, the kernel dispatched it, restored `x0`, returned to EL0, and the user body continued after `svc`.

V34 proves the next piece: the same tiny user process then issues `SYS_EXIT`, the kernel records the exit request, marks the process descriptor as exited, and deliberately does not return to that dead user process.

## Proof chain

```text
EL0 aegis-init
→ SYS_GETPID
→ EL1 syscall dispatch
→ x0 return restored
→ eret back to EL0
→ user process continues after svc
→ SYS_EXIT
→ EL1 exit trap
→ process descriptor marked FIRST_EXITED
→ kernel stop checkpoint
```

## Expected QEMU success

```text
QEMU v34 proof accepted: tiny aegis-init issued SYS_EXIT, kernel marked it exited, and did not return to the dead EL0 process.
```

---

# AegisOS Router Bring-up v35 — User Process Section Complete + ELF Loader

v35 closes the first EL0/user-process bring-up section and adds the first
kernel-side ELF loader contract.

## Proved by this milestone

- v27: VFS/initramfs root, `/dev`, and `/proc` mounted.
- v28: kernel services registered and service manager prepared.
- v29: userland feature catalogue and handoff contract prepared.
- v30: pre-EL0 fake process descriptor, stack/layout, and syscall ABI proof.
- v31: controlled EL0 `eret` transition.
- v32: first tiny `aegis-init` user process entered EL0 and trapped through SVC.
- v33: `SYS_GETPID` returned through saved `x0` and EL0 continued after SVC.
- v34: `SYS_EXIT` marked the process exited and did not return to dead EL0.
- v35: post-exit scheduler handoff marker plus ELF64/AArch64 `/sbin/aegis-init` validation/binding.

## ELF loader scope

This stage validates a minimal static ELF64/AArch64 image contract for
`/sbin/aegis-init`:

- ELF magic/class/data/version checks.
- `ET_EXEC` type check.
- `EM_AARCH64` machine check.
- program-header table bounds check.
- `PT_LOAD` executable segment selection.
- entry-point-inside-load-segment check.
- page alignment checks.
- binding validated ELF entry/text segment to the first user process descriptor.

This is the first real ELF loader contract. File-backed initramfs reads,
page-table-backed mapping/copying, relocations, dynamic linking, and multiple
process images remain later milestones.

## Expected proof line

```text
QEMU v35 proof accepted: post-exit scheduler handoff reached, /sbin/aegis-init ELF was validated/bound, and this first user-process bring-up section is complete.
```

---

# AegisOS Router Bring-up v36 — File-backed Initramfs ELF Loader

V36 moves the first `/sbin/aegis-init` image from a built-in loader buffer to an initramfs/VFS-backed load path.

Proof scope:

- install a minimal ELF64/AArch64 `/sbin/aegis-init` entry into the initramfs file table
- open `/sbin/aegis-init` through VFS
- read ELF bytes through VFS/initramfs
- validate the image with the ELF loader
- bind the parsed entry/text segment to the first user process descriptor
- retain the v31-v35 EL0 launch, syscall, exit, and post-exit scheduler handoff proofs

This is still a loader bring-up proof: real segment copying into mapped EL0 pages and full file-backed process execution come next.

---

# AegisOS Router Bring-up v36.1 — File-backed Initramfs ELF Align Fix

This patch stays on the v36 line. It fixes the first v36 failure mode where QEMU reached VFS open/read and entered ELF validation, but did not reach ELF bind/launch.

Root cause addressed: the VFS loader copied ELF bytes into a raw byte buffer and then validated them through ELF64 header structs. On the bare AArch64 path, that buffer must be aligned before 64-bit header access. v36.1 aligns the loader scratch buffer to 16 bytes.

Expected proof:

```text
QEMU v36 proof accepted: /sbin/aegis-init was opened/read from initramfs through VFS, validated as ELF64/AArch64, bound to the first process, launched, exited, and scheduler handoff remained safe.
```

---

# AegisOS Router Bring-up v37 — PT_LOAD Segment Copy + User/Kernel Permissions

v37 combines the next two loader milestones into one proof stage:

```text
v37:
  copy executable PT_LOAD file bytes into the first user image backing page
  zero the remaining PT_LOAD memory/BSS range
  publish RX user text metadata
  prove RW non-executable heap/stack metadata remains intact
  bind the loaded segment metadata into the first aegis-init descriptor
  retain v31-v36 EL0 launch, syscall, exit, and scheduler handoff proofs
```

## What v37 proves

The v36 loader proved that `/sbin/aegis-init` could be opened/read through VFS/initramfs, validated as ELF64/AArch64, and bound to the first process descriptor.

v37 moves one layer deeper: the loader now performs the first concrete PT_LOAD materialisation step.

```text
VFS read /sbin/aegis-init
→ ELF64/AArch64 validation
→ executable PT_LOAD selected
→ file bytes copied into aligned user-image backing page
→ BSS/tail bytes zeroed
→ text permission metadata published as user + mapped + read + execute
→ no write bit on text
→ heap/stack remain user + read/write + non-exec
→ first process launches/exits through existing proof path
```

## New proof checkpoints

```text
qemu_boot_checkpoint_elf_pt_load_segments_copied
qemu_boot_checkpoint_user_kernel_page_permissions_ready
```

## New validation symbols

```text
copy_executable_pt_load
elf_loader_pt_load_selftest
userland_bind_first_process_to_loaded_elf
userland_pt_load_permissions_selftest
```

## Success line

```text
QEMU v37 proof accepted: PT_LOAD file bytes copied into user image backing, BSS zeroed, user/kernel permissions proved, first process launched/exited safely.
```

## Honest boundary

v37 still does not install real hardware MMU user mappings yet. It proves loader-side materialisation and permission metadata. The next section should turn this metadata into real mapped user pages, then build argv/envp and multiple descriptors before producing the packageable router image.

---

# AegisOS router bring-up v37.1 — PT_LOAD proof-script fix

v37.1 stays on the v37 milestone. It does not add v38 scope.

The v37 runtime path reached the new loader materialisation path:

- `qemu_boot_checkpoint_elf_pt_load_segments_copied`
- `userland_bind_first_process_to_loaded_elf`
- `qemu_boot_checkpoint_user_kernel_page_permissions_ready`
- `userland_pt_load_permissions_selftest`
- EL0 launch/syscall/exit/post-exit handoff

The failure was a stale proof expectation: the QEMU proof still required the old metadata-only binder symbol
`userland_bind_first_process_to_elf`, even though v37 intentionally calls
`userland_bind_first_process_to_loaded_elf` after PT_LOAD bytes have been copied into user backing memory.

## v37.1 fix

- Keeps the v37 PT_LOAD segment copy implementation.
- Keeps user/kernel permission metadata proof.
- Removes the stale trace requirement for `userland_bind_first_process_to_elf`.
- Requires the new loaded-ELF binder path instead.
- Updates the milestone guard/version marker to identify the v37.1 tree.

Expected proof line:

```text
QEMU v37 proof accepted: PT_LOAD file bytes copied into user image backing, BSS zeroed, user/kernel permissions proved, first process launched/exited safely.
```

---

# AegisOS Router Bring-up v38 — argv/envp Stack Bootstrap + Multiple Process Descriptors

v38 combines the two pre-image-packaging userland milestones requested after v37:

```text
v38:
  real argv/envp-style user stack bootstrap metadata
  multiple user process descriptors
```

## What v38 proves

v37 already proved file-backed `/sbin/aegis-init` loading, PT_LOAD copy into the user image backing page, BSS zeroing, and user/kernel permission metadata.

v38 keeps that path and adds:

```text
/sbin/aegis-init PT_LOAD ready
→ build initial argv/envp stack contract
→ set aligned initial user SP from bootstrap stack
→ create secondary descriptors for aegisd, routerd, and rustmyadmin
→ prove descriptor IDs/layouts are unique
→ launch first process through the proven EL0/SVC/exit path
```

## New proof checkpoints

```text
qemu_boot_checkpoint_user_stack_argv_envp_ready
qemu_boot_checkpoint_multiple_user_process_descriptors_ready
```

## New proof functions

```text
userland_prepare_user_stack_bootstrap
userland_user_stack_bootstrap_selftest
userland_create_multiple_process_descriptors
userland_multiple_process_descriptors_selftest
```

## Expected proof line

```text
QEMU v38 proof accepted: argv/envp user stack bootstrap built, multiple process descriptors created, first process launched/exited safely.
```

## Boundary

v38 still uses metadata/backing proof for user mappings. The next planned milestone is the packageable router image path.

## v39 — AegisOS v2.0 Router Image Packaging

V39 turns the proven router bring-up tree into the first packageable AegisOS-v2.0 image stage. It keeps the v27–v38 QEMU-proven kernel/userland chain intact, adds deterministic `.img` packaging tooling, emits image manifests and checksums, and replaces scattered root milestone README files with a consolidated documentation set.

Expected packaging success line:

```text
AegisOS v2.0/v39 image packaging accepted: build/images/AegisOS-v2.0-router-aarch64.img
```

---

## v40 — AegisOS v2.0 Flash Layout + QEMU Disk Image Path + Persistent Config

V40 compresses the next three image/storage milestones into one release step:

```text
real flash image layout
QEMU disk/image boot path
persistent config partition
```

V40 keeps the v35–v38 kernel/userland proof intact, keeps the v39 packageable-image direction, and upgrades the image output into a flash-style raw disk image with an MBR table and stable partition regions.

### New image regions

```text
AEGIS_BOOT
  boot/kernel payload region

AEGIS_ROOT
  reserved root/userland region

AEGIS_CONFIG
  persistent config region
```

### New tools

```text
tools/image/build-aegisos-v2-flash-img.py
tools/image/inspect-aegisos-v2-flash-img.py
tools/qemu/boot-v40-disk-image-aarch64.sh
tools/qemu/prove-v40-disk-image-aarch64.sh
tools/dev/assert-v40-milestone.sh
```

### Expected success lines

```text
AegisOS v2.0/v40 flash image layout accepted: build/images/AegisOS-v2.0-router-aarch64-v40-flash.img
QEMU v40 disk/image boot path accepted: kernel booted with flash image attached: build/images/AegisOS-v2.0-router-aarch64-v40-flash.img
```

### Boundary

V40 proves flash layout generation and QEMU disk attachment. It does not yet implement kernel block-device reads from the attached image, real filesystem mounting from `AEGIS_ROOT`, or firmware bootloader loading directly from `AEGIS_BOOT`.


## v43 — IPC/service messaging

V43 proves service mailbox/channel registration and a kernel-safe `SYS_SEND_MSG` / `SYS_RECV_MSG` message roundtrip between early AegisOS services.


---

## v44 — Service supervisor + fault/page-fault proof

V44 adds the first kernel-side service supervisor on top of the v43 IPC/service messaging layer. The supervisor registers the early process descriptors, transitions them into running state, decodes a controlled lower-EL page-fault-class record, marks `routerd` faulted, stores ESR/FAR/ELR/FSC metadata, and preserves the existing EL0 launch/exit chain.

Expected proof line:

```text
QEMU v44 proof accepted: service supervisor registered early services, recorded controlled page-fault state, marked routerd faulted, and preserved the EL0 launch/exit chain.
```

Boundary: v44 proves service fault accounting and supervisor handoff. v45 is still responsible for real user/kernel page-table isolation.


---

## v45 — User/kernel page-table isolation

V45 adds real per-process TTBR0 page-table roots and explicit user/kernel permission proof. Each early process descriptor receives an isolated user root with EL0 text mapped RX, heap/IPC/stack mapped RW/NX, stack guards left unmapped, and kernel image/stack/high-half probes denied from the user roots. The v44 service supervisor fault boundary and EL0 launch/exit proof chain are preserved.

Expected proof line:

```text
QEMU v45 proof accepted: per-process TTBR0 roots created, EL0 user regions mapped, kernel window blocked, guard pages unmapped, and EL0 launch/exit chain preserved.
```

Boundary: v45 proves isolated roots and PTE permissions. Full multi-service scheduler-driven TTBR0 switching remains a later service runtime step.

---

## v46 — Network bring-up + DHCP + NAT/firewall control plane

V46 adds the first deterministic router control-plane proof on top of v45 user/kernel isolation. The kernel initialises the network scaffolding, records a synthetic DHCP lease, commits IPv4 interface state, installs a default route, loads a default-deny firewall policy with DHCP/DNS/HTTPS egress allowance, enables NAT, and preserves the v45 isolation chain.

Expected proof line:

```text
QEMU v46 proof accepted: network stack brought up, synthetic DHCP lease bound, NAT/firewall control plane loaded, and v45 isolation chain preserved.
```

Boundary: v46 proves the control plane; live packet DHCP and physical NIC validation remain hardware/network-backend work.

---

## v47 — AegisBox Developer Preview IMG

V47 promotes the v39-v46 chain into the AegisBox Developer Preview image contract. The kernel proves Developer Preview manifest readiness, Lite/Pro/Bastion/Router board profile readiness, installer/flash-flow readiness, and preserves the EL0 launch/exit proof chain.

Expected proof line:

```text
QEMU v47 proof accepted: AegisBox Developer Preview IMG contract prepared with Lite/Pro/Bastion/Router variants and EL0 launch/exit chain preserved.
```

Boundary: v47 is the Developer Preview IMG milestone. V48-v54 are reserved for polish, board profiles, installer/flash flow, service configs, security hardening, docs, and image variants before the v55 Release IMG.

---

# AegisOS v48 — Release polish, board profiles, and service config matrix

V48 is the first post-Developer-Preview polish milestone after v47. It proves
that the v47 Developer Preview chain can be extended with a kernel-visible
release polish layer.

## Proved by this milestone

- v46 network/router control-plane state is inherited.
- v47 Developer Preview IMG contract is inherited.
- Lite, Pro, Bastion, and Router board profiles are present.
- Service defaults for early system/router/admin services are present.
- Installer/flash validation, persistent config, docs, and security hardening
  gates are represented in the release-polish manifest.

## Expected proof line

```text
QEMU v48 proof accepted: release polish gates, board profile matrix, service config presets, security hardening baseline, and docs/manifest checks prepared while preserving the v47 Developer Preview chain.
```
