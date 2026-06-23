/* SPDX-License-Identifier: Proprietary */
#pragma once
/* timer.h — ARM generic timer interface */

#include "../../kernel/include/types.h"

int  timer_init(u32 freq_hz);
u64  timer_get_ticks(void);
u64  timer_ticks_to_us(u64 ticks);
void timer_set_oneshot(u64 us, void (*callback)(void));
void timer_set_periodic(u64 us, void (*callback)(void));
