/* SPDX-License-Identifier: Proprietary */
/* AegisOS — hal/arm64/cpu.c
 * AArch64 CPU initialization and utilities.
 */

#include "../include/hal.h"

u32 hal_get_cpu_id(void) {
    u64 mpidr;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    return (u32)(mpidr & 0xFF);
}

u64 hal_get_cpu_freq(void) {
    u64 freq;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    return freq;
}

void hal_reset(void) {
    /* Trigger system reset via PSCI or watchdog */
    __asm__ volatile("smc #0");
}

void hal_poweroff(void) {
    __asm__ volatile("smc #0");
}

int hal_init(void) {
    /* Enable floating-point / NEON */
    u64 cpacr;
    __asm__ volatile("mrs %0, cpacr_el1" : "=r"(cpacr));
    cpacr |= (3ULL << 20);
    __asm__ volatile("msr cpacr_el1, %0" :: "r"(cpacr));
    __asm__ volatile("isb");
    return 0;
}
