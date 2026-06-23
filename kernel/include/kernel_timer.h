/* SPDX-License-Identifier: Proprietary */
#pragma once
#include "types.h"

void kernel_timer_init(void);
int  ktimer_create(u64 delay_us, void (*callback)(void *data), void *data, bool periodic);
void ktimer_cancel(int timer_id);
u64  kernel_get_ticks(void);
u64  kernel_get_jiffies(void);
void kernel_sleep_us(u64 us);
int  kernel_timer_bringup_selftest(void);
