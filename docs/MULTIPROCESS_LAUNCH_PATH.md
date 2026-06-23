# AegisOS v42 — Real Multi-Process Launch Path

v42 promotes the v38 process descriptors from static metadata into a real kernel launch contract.

The launch plan covers:

- `aegis-init` as PID 1 / first process.
- `aegisd` as the base service daemon.
- `routerd` as the router control-plane service.
- `rustmyadmin` as the future admin handoff surface.

Each descriptor owns a non-overlapping text, heap, IPC, stack, and guard layout. v42 does not yet run all EL0 tasks concurrently; that scheduler step belongs to later service-supervision work. It does prove that all descriptors are launchable, stack-bootstrapped, and syscall-addressable.
