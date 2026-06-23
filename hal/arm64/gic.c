/* SPDX-License-Identifier: Proprietary */
/* AegisOS — hal/arm64/gic.c
 * ARM GICv2 interrupt controller driver.
 */

#include "../../kernel/include/types.h"

#define GICD_BASE   0x08000000UL
#define GICC_BASE   0x08010000UL

#define GICD_CTLR       (*(volatile u32 *)(GICD_BASE + 0x000))
#define GICD_ISENABLER  ((volatile u32 *)(GICD_BASE + 0x100))
#define GICC_CTLR       (*(volatile u32 *)(GICC_BASE + 0x000))
#define GICC_PMR        (*(volatile u32 *)(GICC_BASE + 0x004))
#define GICC_IAR        (*(volatile u32 *)(GICC_BASE + 0x00C))
#define GICC_EOIR       (*(volatile u32 *)(GICC_BASE + 0x010))

void gic_init(void) {
    GICD_CTLR = 1;   /* Enable Distributor */
    GICC_PMR  = 0xFF; /* Allow all priorities */
    GICC_CTLR = 1;   /* Enable CPU interface */
}

void gic_enable_irq(u32 irq) {
    GICD_ISENABLER[irq / 32] = 1U << (irq % 32);
}

u32 gic_ack_irq(void) {
    return GICC_IAR & 0x3FF;
}

void gic_eoi_irq(u32 irq) {
    GICC_EOIR = irq;
}
