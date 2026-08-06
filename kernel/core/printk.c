/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/core/printk.c
 * Minimal kernel formatted print over UART.
 */

#include "types.h"
#include <stdarg.h>

/* Forward declaration — implemented in hal/drivers/uart */
extern void uart_putchar(char c);

static void printk_str(const char *s) {
    while (*s) uart_putchar(*s++);
}

static void printk_uint(u64 v, int base) {
    static const char digits[] = "0123456789abcdef";
    char buf[20];
    int  i = 0;
    if (v == 0) { uart_putchar('0'); return; }
    while (v) { buf[i++] = digits[v % base]; v /= base; }
    while (i--) uart_putchar(buf[i]);
}

void printk(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    for (; *fmt; fmt++) {
        if (*fmt != '%') { uart_putchar(*fmt); continue; }
        fmt++;
        switch (*fmt) {
        case 'd': { s64 v = va_arg(ap, int); if (v < 0) { uart_putchar('-'); v = -v; } printk_uint((u64)v, 10); break; }
        case 'u': printk_uint(va_arg(ap, unsigned int), 10); break;
        case 'x': printk_uint(va_arg(ap, unsigned int), 16); break;
        case 'p': printk_str("0x"); printk_uint((u64)va_arg(ap, void *), 16); break;
        case 's': printk_str(va_arg(ap, const char *)); break;
        case '%': uart_putchar('%'); break;
        default:  uart_putchar('%'); uart_putchar(*fmt); break;
        }
    }
    va_end(ap);
}
