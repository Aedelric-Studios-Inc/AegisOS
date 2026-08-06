/* SPDX-License-Identifier: Proprietary */
#pragma once
/* AegisOS early boot logging.
 *
 * This is deliberately tiny and freestanding.  It can run before the full HAL
 * console stack is up, mirrors messages into a small in-memory ring buffer,
 * writes directly to the QEMU/Bastion PL011 when configured, and emits QEMU
 * semihosting checkpoints in smoke builds.
 */

#include "types.h"

void early_log_init(uptr uart_base);
void early_log_putchar(char c);
void early_log_puts(const char *s);
void early_log_hex64(u64 value);
void early_log_checkpoint(const char *message);
const char *early_log_buffer(void);
u64 early_log_length(void);
