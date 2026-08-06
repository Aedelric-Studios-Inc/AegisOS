# AegisOS QEMU boot proof v25 — timer and scheduler

This stage keeps the v23 proof chain and adds two focused proofs:

1. **Timer proof** — validates the ARM generic counter is readable/monotonic and that the physical timer can be programmed without depending on IRQ delivery yet.
2. **Scheduler proof** — validates scheduler initialisation, task allocation, ready-list growth, unique TIDs, saved entry addresses, and stack allocation without starting the first real kernel init thread.

The first real kernel init thread is deliberately deferred to the next milestone so scheduler handoff can be debugged in isolation.
