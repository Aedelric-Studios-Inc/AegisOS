# AegisOS v56 native PID 1 IRQ preemption gate

The v56 native PID 1 guest could stop during initramfs population immediately
after the first ARM physical-timer interrupt.

The timer IRQ was rearmed and acknowledged correctly, but the dispatcher still
called the cooperative scheduler from inside the exception handler. The current
`context_switch` saves task callee-saved registers and a stack pointer; it does
not transfer the complete exception frame or restore a new task through `eret`.
A switch from the IRQ handler therefore started the idle task while the CPU was
still in IRQ-masked EL1 state. Its `WFI` could not take the next timer IRQ, so
the guest stopped at whichever init step happened to be interrupted.

This gate keeps timer IRQ timekeeping and software-timer callbacks active but
disables IRQ-driven task switching. Cooperative `scheduler_yield()` paths remain
available. Native preemption must be re-enabled only after the scheduler owns a
full saved exception frame per task and switches the selected frame at exception
return.
