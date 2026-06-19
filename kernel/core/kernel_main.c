/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/core/kernel_main.c
 * Kernel entry point called from boot/arm64/start.S.
 */

#include "aegis_kernel.h"
#include "hal.h"

extern int  board_init(void);
extern void gic_init(void);

void kernel_main(void) {
    irq_enable_set_allowed(false);
    irq_global_disable();

    if (hal_init() != 0) PANIC("hal_init() failed");
    if (board_init() != 0) PANIC("board_init() failed");
    gic_init();

    /* Initialize physical memory allocator */
    phys_mem_init(0x40000000UL, 0x40000000UL);  /* 1 GB starting at 0x40000000 */

    /* Initialize virtual memory and page tables */
    page_table_init();
    virt_mem_init();

    /* Initialize kernel heap */
    heap_init(0xFFFFFF8800000000UL, 64UL * 1024 * 1024);

    /* Initialize scheduler */
    scheduler_init();

    /* Initialize syscall table */
    syscall_table_init();

    /* Print boot banner */
    printk("[AegisOS] Kernel started — v%d.%d.%d\n",
           AEGISOS_VERSION_MAJOR,
           AEGISOS_VERSION_MINOR,
           AEGISOS_VERSION_PATCH);

    irq_enable_set_allowed(true);
    if (!irq_try_global_enable() || !irq_global_is_enabled()) {
        PANIC("Failed to safely enable IRQs");
    }

    /* Start scheduler — does not return under normal operation */
    scheduler_run();

    PANIC("scheduler_run() returned unexpectedly");
}
