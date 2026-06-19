/* SPDX-License-Identifier: Proprietary */
#pragma once
/* panic.h — kernel panic interface */

#include "types.h"

#define PANIC(msg) kernel_panic((msg), __FILE__, __LINE__)

__attribute__((noreturn))
void kernel_panic(const char *msg, const char *file, int line);

void panic_dump_regs(void);
