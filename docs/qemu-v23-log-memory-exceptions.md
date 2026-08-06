# AegisOS v23 — visible log, memory selftest, exception vector proof

v23 wires the first three real kernel bring-up targets after the v22 boot spine:

1. **Visible/early logging path**
   - Adds `kernel/core/early_log.c` and `kernel/include/early_log.h`.
   - Logs to a bounded PL011 path and mirrors boot messages into an in-memory early log buffer.
   - Emits QEMU semihost checkpoints in smoke builds so the trace harness can prove log execution even when host stdout is swallowed.

2. **Memory phase selftest**
   - Extends the physical allocator with reserved ranges and memory statistics.
   - Reserves QEMU stub window, kernel image, kernel stack, heap window, and FDT area.
   - Allocates/frees physical pages and validates heap allocation/write/free behaviour.

3. **Exception vector proof**
   - Adds `exception_vector_selftest()`.
   - Verifies `VBAR_EL1` points at `_vectors` and that the vector table is correctly 2 KiB aligned.
   - Adds a QEMU trace checkpoint for exception-vector proof.

Run:

```sh
cd ~/AegisOS
tools/dev/fix-clock-skew.sh
tools/qemu/build-and-smoke-aarch64.sh
```

Expected proof includes:

```text
qemu boot proof: reached qemu_boot_checkpoint_visible_log
qemu boot proof: reached qemu_boot_checkpoint_memory_selftest
qemu boot proof: reached qemu_boot_checkpoint_exception_vectors
QEMU v23 proof accepted: visible logging, memory selftest, and exception vectors reached.
```
