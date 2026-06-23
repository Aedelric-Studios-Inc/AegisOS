/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/core/timer.c
 * Kernel timer subsystem — manages software timers backed by the hardware
 * ARM generic timer (hal/arm64/timer.c).
 */

#include "types.h"
#include "memory.h"
#include "scheduler.h"
#include "timer.h"
#include "hal.h"

#define MAX_TIMERS 256
#define TIMER_IRQ  30  /* ARM PPI: physical timer */

typedef struct ktimer {
    u64  expire_ticks;
    void (*callback)(void *data);
    void *data;
    bool active;
    bool periodic;
    u64  interval_us;
} ktimer_t;

static ktimer_t timers[MAX_TIMERS];
static u64 tick_count;
static u64 jiffies;

/* Called from IRQ handler on timer tick */
static void timer_irq_handler(void) {
    tick_count++;
    jiffies++;

    /* Check for expired timers */
    u64 now = timer_get_ticks();
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (!timers[i].active) continue;
        if (now >= timers[i].expire_ticks) {
            timers[i].active = false;
            if (timers[i].callback) {
                timers[i].callback(timers[i].data);
            }
            /* Re-arm periodic timers */
            if (timers[i].periodic) {
                u64 freq;
                __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
                timers[i].expire_ticks = now + (timers[i].interval_us * (freq / 1000000ULL));
                timers[i].active = true;
            }
        }
    }

    /* Trigger preemptive scheduling on every tick */
    scheduler_yield();
}

void kernel_timer_init(void) {
    for (int i = 0; i < MAX_TIMERS; i++) {
        timers[i].active = false;
    }
    tick_count = 0;
    jiffies = 0;

    /* Register timer IRQ and set up periodic tick at 10ms (100 Hz) */
    extern void irq_register(u32 irq, void (*handler)(u32, void*), void *data);
    extern void gic_enable_irq(u32 irq);

    irq_register(TIMER_IRQ, (void(*)(u32, void*))timer_irq_handler, NULL);
    gic_enable_irq(TIMER_IRQ);
    timer_set_periodic(10000, timer_irq_handler); /* 10ms tick */
}

int ktimer_create(u64 delay_us, void (*callback)(void *data), void *data, bool periodic) {
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (!timers[i].active) {
            u64 freq;
            __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
            u64 now = timer_get_ticks();
            timers[i].expire_ticks = now + (delay_us * (freq / 1000000ULL));
            timers[i].callback = callback;
            timers[i].data = data;
            timers[i].periodic = periodic;
            timers[i].interval_us = delay_us;
            timers[i].active = true;
            return i;
        }
    }
    return AEGIS_ENOMEM;
}

void ktimer_cancel(int timer_id) {
    if (timer_id >= 0 && timer_id < MAX_TIMERS) {
        timers[timer_id].active = false;
    }
}

u64 kernel_get_ticks(void) {
    return tick_count;
}

u64 kernel_get_jiffies(void) {
    return jiffies;
}

void kernel_sleep_us(u64 us) {
    u64 start = timer_get_ticks();
    u64 end = start + (us * (hal_get_cpu_freq() / 1000000ULL));
    while (timer_get_ticks() < end) {
        scheduler_yield();
    }
}


int kernel_timer_bringup_selftest(void) {
    u64 freq = hal_get_cpu_freq();
    if (freq == 0 || freq < 1000000ULL) {
        return AEGIS_EINVAL;
    }

    u64 before = timer_get_ticks();
    for (volatile u64 spin = 0; spin < 10000ULL; spin++) {
        __asm__ volatile("nop" ::: "memory");
    }
    u64 after = timer_get_ticks();
    if (after <= before) {
        return AEGIS_EBUSY;
    }

    u64 delta_us = timer_ticks_to_us(after - before);
    (void)delta_us;

    timer_set_oneshot(1000, NULL);

    u64 ctl = 0;
    u64 tval = 0;
    __asm__ volatile("mrs %0, cntp_ctl_el0" : "=r"(ctl));
    __asm__ volatile("mrs %0, cntp_tval_el0" : "=r"(tval));

    /* Disable the physical timer after the proof so the smoke harness does not
     * depend on IRQ delivery yet.  Interrupt-driven ticks come later.
     */
    __asm__ volatile("msr cntp_ctl_el0, %0" :: "r"((u64)0) : "memory");
    __asm__ volatile("isb" ::: "memory");

    if ((ctl & 1ULL) == 0 || tval == 0) {
        return AEGIS_EINVAL;
    }

    return AEGIS_OK;
}
