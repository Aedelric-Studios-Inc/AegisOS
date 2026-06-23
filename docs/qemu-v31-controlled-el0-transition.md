# AegisOS v31.0 — Controlled EL0 Transition Probe

v31.0 keeps the milestone locked at v31 until the EL0 transition is proven. It
adds the first controlled privilege drop from EL1 to EL0, but it does not launch
a real ELF/user process yet.

## Scope

- Prepare the existing v30 fake `aegis-init` user descriptor.
- Reuse the v30 stack/address-space contract.
- Select a tiny kernel-resident EL0 probe stub.
- Use `eret` from EL1 into EL0t with DAIF masked.
- Execute the EL0 stub.
- Trap back to EL1 through `svc #0` and the `el0_sync_a64` vector.
- Mark the trap from `syscall_handler()` and hold the CPU for QEMU proof.

## Deliberate deferrals

- No ELF loader yet.
- No user page table isolation yet.
- No real user process exit/continuation yet.
- No return-to-kernel landing path after the SVC trap yet.

Those belong to v32 after the controlled transition is stable.
