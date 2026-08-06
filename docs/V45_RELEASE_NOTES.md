# AegisOS v45 — User/kernel page-table isolation proof

AegisOS v45 adds the first real per-process page-table isolation layer on top of the v44 service supervisor and fault proof.

## Scope

v45 proves:

- per-process TTBR0 page-table roots for the early userland service set
- EL0-readable/executable user text pages
- EL0-readable/writable but non-executable heap, IPC, and stack pages
- unmapped stack guard pages
- kernel image/stack/high-half probes denied from user roots
- TTBR0 switch contract metadata prepared for future scheduler integration
- v44 supervisor fault boundary preserved before EL0 launch/exit proof

## Expected QEMU proof line

```text
QEMU v45 proof accepted: per-process TTBR0 roots created, EL0 user regions mapped, kernel window blocked, guard pages unmapped, and EL0 launch/exit chain preserved.
```

## Boundary

v45 prepares and proves real user page-table roots and PTE permissions. The running kernel still preserves the proven v44/v43 EL0 chain and does not yet perform a full scheduler-driven TTBR0 context switch for multiple live user services. That belongs to the post-v45 service runtime work.
