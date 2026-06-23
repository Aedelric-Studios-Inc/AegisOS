# AegisOS v2.0 v44 Release Notes — Service Supervisor + Fault/Page-Fault Proof

AegisOS v44 advances the v43 IPC/service messaging milestone into the first supervised service-fault boundary.

V44 does not attempt full user/kernel page-table isolation yet. That remains v45. Instead, v44 proves that the kernel can register early userland services with a supervisor, classify a controlled page-fault record, mark the affected service faulted, preserve fault metadata, and keep the EL0 launch/exit proof chain intact.

## Added

- `kernel/include/service_supervisor.h`
- `kernel/core/service_supervisor.c`
- v44 service supervisor state model
- supervised service table for early process descriptors
- service states: registered, starting, running, faulted, restarting, stopped
- service fault kinds: exit, page-fault, watchdog, IPC
- fault record metadata: ESR, FAR, ELR, EC, FSC, lower-EL/write/page-fault classification
- controlled `routerd` page-fault proof path
- QEMU proof checkpoints for supervisor readiness, page-fault recording, faulted service state, and handoff preservation
- `tools/dev/assert-v44-milestone.sh`

## Exception decode support

`kernel/core/exception.c` now exposes a fault decoder used by the supervisor proof:

```c
int exception_decode_fault(u64 esr, u64 far, u64 elr, aegis_exception_fault_t *out);
bool exception_fault_is_user_page_fault(const aegis_exception_fault_t *fault);
```

The v44 proof uses a controlled lower-EL data-abort record and verifies that it is page-fault class before the supervisor records it.

## QEMU checkpoints

```text
qemu_boot_checkpoint_v44_service_supervisor_ready
qemu_boot_checkpoint_v44_fault_injection_ready
qemu_boot_checkpoint_v44_page_fault_trap_recorded
qemu_boot_checkpoint_v44_service_marked_faulted
qemu_boot_checkpoint_v44_supervisor_handoff_preserved
```

## Target proof line

```text
QEMU v44 proof accepted: service supervisor registered early services, recorded controlled page-fault state, marked routerd faulted, and preserved the EL0 launch/exit chain.
```

## Boundary

V44 is a deterministic supervisor/fault-accounting milestone. It proves the kernel-side control plane for service fault handling. It does not yet enforce real hardware-backed user/kernel isolation. That is the v45 target.
