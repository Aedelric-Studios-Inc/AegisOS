/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/core/irq_dispatch.c
 * IRQ handler dispatch table.
 */

#include "types.h"

#define MAX_IRQS 1024

typedef void (*irq_handler_t)(u32 irq, void *data);

typedef struct {
    irq_handler_t handler;
    void         *data;
} irq_entry_t;

static irq_entry_t irq_table[MAX_IRQS];

void irq_register(u32 irq, irq_handler_t handler, void *data) {
    if (irq >= MAX_IRQS) return;
    irq_table[irq].handler = handler;
    irq_table[irq].data    = data;
}

void irq_dispatch(void *regs) {
    (void)regs;
    /* Query GIC for pending IRQ number */
    extern u32 gic_ack_irq(void);
    extern void gic_eoi_irq(u32);

    u32 irq = gic_ack_irq();
    if (irq < MAX_IRQS && irq_table[irq].handler) {
        irq_table[irq].handler(irq, irq_table[irq].data);
    }
    gic_eoi_irq(irq);
}
