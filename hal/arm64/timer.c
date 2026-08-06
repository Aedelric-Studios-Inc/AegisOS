/* SPDX-License-Identifier: Proprietary */
/* AegisOS — hal/arm64/timer.c
 * ARM generic timer (AArch64 system timer).
 */

#include "../include/timer.h"

static u32 timer_freq_hz;
static void (*timer_callback)(void);
static u64 timer_period_us;
static bool timer_periodic_enabled;

static void timer_program_delta(u64 us) {
    u64 ticks_per_us = timer_freq_hz / 1000000ULL;
    if (ticks_per_us == 0) ticks_per_us = 1;
    u64 tval = us * ticks_per_us;
    if (tval == 0) tval = 1;
    __asm__ volatile("msr cntp_tval_el0, %0" :: "r"(tval) : "memory");
    __asm__ volatile("msr cntp_ctl_el0, %0" :: "r"((u64)1) : "memory");
    __asm__ volatile("isb" ::: "memory");
}

int timer_init(u32 freq_hz) {
    timer_freq_hz = freq_hz;
    /* Read actual frequency from system register */
    u64 actual_freq;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(actual_freq));
    if (actual_freq == 0) actual_freq = freq_hz;
    timer_freq_hz = (u32)actual_freq;
    timer_callback = 0;
    timer_period_us = 0;
    timer_periodic_enabled = false;
    return 0;
}

u64 timer_get_ticks(void) {
    u64 ticks;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(ticks));
    return ticks;
}

u64 timer_ticks_to_us(u64 ticks) {
    return ticks / (timer_freq_hz / 1000000ULL);
}

void timer_set_oneshot(u64 us, void (*callback)(void)) {
    timer_callback = callback;
    timer_periodic_enabled = false;
    timer_period_us = 0;
    timer_program_delta(us);
}

void timer_set_periodic(u64 us, void (*callback)(void)) {
    timer_callback = callback;
    timer_period_us = us;
    timer_periodic_enabled = true;
    timer_program_delta(us);
}

void timer_rearm_periodic(void) {
    if (!timer_periodic_enabled || timer_period_us == 0) return;
    timer_program_delta(timer_period_us);
}
