/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/core/interrupts.c
 * Global interrupt mask control helpers for EL1.
 */

#include "interrupts.h"

static volatile bool irq_enable_allowed = false;

void irq_global_disable(void) {
    __asm__ volatile("msr daifset, #0x2" ::: "memory");
    __asm__ volatile("isb");
}

bool irq_global_is_enabled(void) {
    u64 daif = 0;
    __asm__ volatile("mrs %0, daif" : "=r"(daif));
    return ((daif >> 7) & 1U) == 0;
}

bool irq_try_global_enable(void) {
    if (!irq_enable_allowed) return false;
    __asm__ volatile("dsb sy" ::: "memory");
    __asm__ volatile("msr daifclr, #0x2" ::: "memory");
    __asm__ volatile("isb");
    return true;
}

void irq_enable_set_allowed(bool allowed) {
    irq_enable_allowed = allowed;
    if (!allowed) irq_global_disable();
}

bool irq_enable_is_allowed(void) {
    return irq_enable_allowed;
}
