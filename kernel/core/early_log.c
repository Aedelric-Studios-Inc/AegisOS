/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/core/early_log.c */

#include "early_log.h"

#define PL011_DR(base) (*(volatile u32 *)((base) + 0x00))
#define PL011_FR(base) (*(volatile u32 *)((base) + 0x18))
#define PL011_IBRD(base) (*(volatile u32 *)((base) + 0x24))
#define PL011_FBRD(base) (*(volatile u32 *)((base) + 0x28))
#define PL011_LCRH(base) (*(volatile u32 *)((base) + 0x2c))
#define PL011_CR(base) (*(volatile u32 *)((base) + 0x30))
#define PL011_FR_TXFF (1u << 5)

static uptr early_uart_base;
static char early_buffer[8192];
static u64 early_buffer_len;

static void early_buffer_char(char c) {
    if (early_buffer_len + 1 < sizeof(early_buffer)) {
        early_buffer[early_buffer_len++] = c;
        early_buffer[early_buffer_len] = '\0';
    }
}

void early_log_init(uptr uart_base) {
    early_uart_base = uart_base;
    early_buffer_len = 0;
    early_buffer[0] = '\0';

    if (early_uart_base != 0) {
        PL011_CR(early_uart_base) = 0;
        PL011_IBRD(early_uart_base) = 13;   /* 24 MHz / 115200 */
        PL011_FBRD(early_uart_base) = 21;
        PL011_LCRH(early_uart_base) = 0x70;
        PL011_CR(early_uart_base) = 0x301;  /* UARTEN | TXE | RXE */
    }
}

void early_log_putchar(char c) {
    early_buffer_char(c);

    if (early_uart_base == 0) return;

    /* Bounded wait.  Early logging must never permanently wedge boot. */
    u32 spins = 0x10000;
    while ((PL011_FR(early_uart_base) & PL011_FR_TXFF) && spins) {
        spins--;
    }
    if (spins) {
        PL011_DR(early_uart_base) = (u32)(u8)c;
    }
}

void early_log_puts(const char *s) {
    if (!s) return;
    while (*s) early_log_putchar(*s++);
}

void early_log_hex64(u64 value) {
    static const char digits[] = "0123456789abcdef";
    early_log_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        early_log_putchar(digits[(value >> shift) & 0xf]);
    }
}

void early_log_checkpoint(const char *message) {
    early_log_puts(message);
#ifdef AEGISOS_QEMU_SMOKE
    /* SYS_WRITE0.  Trace proof sees this even if host stdout is swallowed. */
    __asm__ volatile(
        "mov x0, #4\n"
        "mov x1, %0\n"
        "hlt #0xf000\n"
        :: "r"(message)
        : "x0", "x1", "memory");
#endif
}

const char *early_log_buffer(void) {
    return early_buffer;
}

u64 early_log_length(void) {
    return early_buffer_len;
}
