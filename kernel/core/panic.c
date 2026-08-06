/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/core/panic.c */

#include "panic.h"
#include "types.h"

__attribute__((noreturn))
void kernel_panic(const char *msg, const char *file, int line) {
    /* Disable interrupts */
    __asm__ volatile("msr daifset, #0xf" ::: "memory");

    printk("\n[PANIC] %s\n  at %s:%d\n", msg, file, line);
    panic_dump_regs();

    /* Halt */
    for (;;) {
        __asm__ volatile("wfe");
    }
}

void panic_dump_regs(void) {
    /* Register dump would be populated by exception handler context */
    printk("[PANIC] Register dump not yet implemented\n");
}
