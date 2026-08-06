# AegisOS v57 native EL0 context-restore hotfix

The first v57 supervisor-chain build could reach PID 3 and print
`[aegisd] native IPC endpoint registered`, but PID 2 did not resume to consume
the queued health message.

Two cooperative-scheduling defects met at that point:

1. `context_switch` preserved the kernel stack and callee-saved GPRs, but not
   the CPU-global `ELR_EL1`, `SPSR_EL1`, or `SP_EL0` registers belonging to a
   userspace task suspended inside an SVC exception.
2. Round-robin yield could select the idle task from an SVC path while IRQs
   were masked, causing `WFI` to sleep without a wakeable interrupt.

The hotfix stores and restores the complete EL0 exception-return state in each
task context, marks the scheduler's idle task explicitly, skips idle whenever a
normal task is ready, and guards idle `WFI` when IRQs are masked.

The runtime acceptance line remains:

```text
[service-manager] aegisd IPC health check passed
```

This does not re-enable timer-IRQ preemption. Scheduling remains cooperative.
