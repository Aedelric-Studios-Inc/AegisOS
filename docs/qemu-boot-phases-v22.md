# AegisOS v22 — Subsystem Boot Proof

v22 moves beyond proving that `_start`, `_aegisos_reset_entry`, `kernel_main`,
and `boot_phase_enter` execute.  The QEMU proof harness now validates ordered
kernel subsystem progression by matching non-inlined checkpoint symbols in the
instruction trace.

Validated checkpoints:

- `qemu_boot_checkpoint_kernel_main`
- `qemu_boot_checkpoint_memory`
- `qemu_boot_checkpoint_core`
- `qemu_boot_checkpoint_devices`
- `qemu_boot_checkpoint_kernel_services`
- `qemu_boot_checkpoint_security`
- `qemu_boot_checkpoint_network`
- `qemu_boot_checkpoint_init`
- `qemu_boot_checkpoint_running`

These checkpoints intentionally use symbols rather than stdout banners because
some QEMU/terminal combinations swallow PL011 or semihost text. The trace is the
source of truth for early boot progression.

The next target after v22 is to replace placeholder service hooks with concrete
kernel module registration and to prove the scheduler can run the first init
thread.
