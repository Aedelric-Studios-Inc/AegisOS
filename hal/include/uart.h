/* SPDX-License-Identifier: Proprietary */
#pragma once
/* uart.h — UART driver interface */

#include "../../kernel/include/types.h"

typedef struct {
    uptr base_addr;
    u32  baud_rate;
    u32  clock_hz;
} uart_config_t;

int  uart_init(const uart_config_t *cfg);
void uart_putchar(char c);
int  uart_getchar(void);
bool uart_rx_ready(void);
void uart_puts(const char *s);
