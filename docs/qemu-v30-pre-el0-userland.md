# AegisOS v30 — Pre-EL0 Userland Contract Proof

v30 combines the next three userland preparation milestones while intentionally
stopping before the real EL0 transition:

1. fake first user process descriptor
2. user stack and address-space layout proof
3. syscall ABI proof while still in kernel-safe mode

## What v30 proves

During `aegisbox-init`, after the v29 userland handoff contract is ready, the
kernel now prepares a controlled pre-EL0 contract for the future first userland
process.

The first fake process descriptor is for:

- feature: `aegis-init`
- image: `/sbin/aegis-init`
- entry VA: `0x0000000000400000`
- stack top: `0x0000007fff000000`
- syscall gate: `0x0000007fffff0000`

No executable is launched yet. No exception return into EL0 happens in v30.

## Layout checked

The selftest verifies that the user text, heap, IPC window, stack, and guard
page are page-aligned and non-overlapping. It also verifies that the initial SP
is 16-byte aligned and inside the user stack region.

## Syscall ABI checked

The syscall proof stays in kernel-safe mode. It validates syscall dispatch,
invalid syscall rejection, and side-effect-light argument validation paths
without copying from EL0 memory or yielding out of the current task.

## QEMU proof checkpoints

The QEMU trace proof now requires these additional checkpoint symbols:

- `qemu_boot_checkpoint_fake_user_process_descriptor`
- `qemu_boot_checkpoint_user_stack_layout`
- `qemu_boot_checkpoint_syscall_abi_kernel_safe`
- `qemu_boot_checkpoint_pre_el0_contract_ready`

Expected final proof line:

```text
QEMU v30 proof accepted: fake user process descriptor, user stack/address-space layout, and kernel-safe syscall ABI proof reached; controlled EL0 transition intentionally deferred.
```

## What remains for v31

v31 should be the controlled EL0 transition test:

- build an EL0 test frame
- set user SPSR/ELR/SP state safely
- verify page permissions
- return into EL0 in a tiny controlled test
- trap back through syscall/exception path

