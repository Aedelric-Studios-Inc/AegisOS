# AegisOS v26 — First Kernel Init Thread Proof

v26 proves the transition from boot-time C coordination into the first
scheduler-managed kernel task.

## What changed

- `task_t` now records `stack_base` and `stack_size` so proof tasks can be
  safely removed after validation.
- `scheduler_run()` now selects the highest-priority ready task rather than
  blindly starting the list head.
- Scheduler selftests no longer leave proof tasks in the runnable queue.
- `aegisbox-init` is created as the highest-priority initial kernel task.
- In QEMU smoke mode, boot now calls `scheduler_run()` instead of halting before
  the scheduler.
- `aegisbox_init_task()` emits the traceable checkpoint
  `qemu_boot_checkpoint_first_init_thread()` as soon as it begins executing.

## Expected proof line

```text
qemu boot proof: reached qemu_boot_checkpoint_first_init_thread
QEMU v26 proof accepted: first kernel init thread entered under scheduler control.
```

## Meaning

This proves:

```text
_start
  -> ARM64 reset entry
  -> kernel_main
  -> boot phases
  -> scheduler_run
  -> context_switch
  -> aegisbox-init task entry
```

That is the first scheduler-owned execution handoff in AegisOS.
