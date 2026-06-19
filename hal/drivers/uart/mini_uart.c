/* SPDX-License-Identifier: Proprietary */
/* AegisOS — hal/drivers/uart/mini_uart.c
 * BCM2835 Mini UART (AUX UART) driver for Raspberry Pi-compatible boards.
 */

#include "../../include/uart.h"

#define AUX_BASE      0xFE215000UL
#define AUX_ENABLES   (*(volatile u32 *)(AUX_BASE + 0x04))
#define AUX_MU_IO     (*(volatile u32 *)(AUX_BASE + 0x40))
#define AUX_MU_IER    (*(volatile u32 *)(AUX_BASE + 0x44))
#define AUX_MU_IIR    (*(volatile u32 *)(AUX_BASE + 0x48))
#define AUX_MU_LCR    (*(volatile u32 *)(AUX_BASE + 0x4C))
#define AUX_MU_MCR    (*(volatile u32 *)(AUX_BASE + 0x50))
#define AUX_MU_LSR    (*(volatile u32 *)(AUX_BASE + 0x54))
#define AUX_MU_CNTL   (*(volatile u32 *)(AUX_BASE + 0x60))
#define AUX_MU_BAUD   (*(volatile u32 *)(AUX_BASE + 0x68))

int mini_uart_init(u32 clock_hz, u32 baud_rate) {
    AUX_ENABLES = 1;
    AUX_MU_IER  = 0;
    AUX_MU_CNTL = 0;
    AUX_MU_LCR  = 3;   /* 8-bit */
    AUX_MU_MCR  = 0;
    AUX_MU_IIR  = 0xC6;
    AUX_MU_BAUD = clock_hz / (8 * baud_rate) - 1;
    AUX_MU_CNTL = 3;   /* TX + RX enable */
    return 0;
}
