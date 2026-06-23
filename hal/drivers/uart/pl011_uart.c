/* SPDX-License-Identifier: Proprietary */
/* AegisOS — hal/drivers/uart/pl011_uart.c
 * ARM PL011 UART driver.
 */

#include "../../include/uart.h"

#define PL011_DR(b)   (*(volatile u32 *)((b) + 0x00))
#define PL011_FR(b)   (*(volatile u32 *)((b) + 0x18))
#define PL011_IBRD(b) (*(volatile u32 *)((b) + 0x24))
#define PL011_FBRD(b) (*(volatile u32 *)((b) + 0x28))
#define PL011_LCRH(b) (*(volatile u32 *)((b) + 0x2C))
#define PL011_CR(b)   (*(volatile u32 *)((b) + 0x30))

#define FR_TXFF (1 << 5)
#define FR_RXFE (1 << 4)

static uptr uart_base;
static bool uart_ready;

int uart_init(const uart_config_t *cfg) {
    uart_base = cfg->base_addr;
    uart_ready = true;
    PL011_CR(uart_base) = 0;  /* Disable UART */

    /* Baud rate divisors: BRD = clock / (16 * baud) */
    u32 brd = cfg->clock_hz / (16 * cfg->baud_rate);
    PL011_IBRD(uart_base) = brd;
    PL011_FBRD(uart_base) = ((cfg->clock_hz % (16 * cfg->baud_rate)) * 64 + cfg->baud_rate / 2) / cfg->baud_rate;

    PL011_LCRH(uart_base) = 0x70;  /* 8N1, FIFO enabled */
    PL011_CR(uart_base)   = 0x301; /* RXE | TXE | UARTEN */
    return 0;
}

void uart_putchar(char c) {
    if (!uart_ready || uart_base == 0) return;

    /* Never let printk wedge boot forever during early bring-up. */
    u32 spins = 0x10000;
    while ((PL011_FR(uart_base) & FR_TXFF) && spins) {
        spins--;
    }
    if (spins) {
        PL011_DR(uart_base) = (u32)(unsigned char)c;
    }
}

int uart_getchar(void) {
    if (PL011_FR(uart_base) & FR_RXFE) return -1;
    return (int)(PL011_DR(uart_base) & 0xFF);
}

bool uart_rx_ready(void) {
    return !(PL011_FR(uart_base) & FR_RXFE);
}

void uart_puts(const char *s) {
    while (*s) uart_putchar(*s++);
}
