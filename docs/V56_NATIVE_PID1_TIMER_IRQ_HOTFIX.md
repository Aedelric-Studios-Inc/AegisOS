# AegisOS v56 native PID 1 timer/IRQ hotfix

The first native PID 1 build could stop after the kernel service-manager
self-test. The native bootstrap itself had not begun.

The cause was the scheduler tick path:

1. the ARM generic physical timer was programmed as a one-shot even though the
   HAL function was named `timer_set_periodic`;
2. the timer IRQ handler called `scheduler_yield()` before `gic_eoi_irq()`;
3. the init task could therefore switch to the idle task while IRQ 30 remained
   active;
4. the idle task entered `WFI`, but the same PPI could not deliver another tick
   before EOI, leaving the guest frozen.

This hotfix implements software periodic rearming, records a deferred
reschedule request in the timer handler, acknowledges/EOIs the IRQ first, and
only then invokes the scheduler. It also removes the incompatible function
pointer cast from timer IRQ registration.
