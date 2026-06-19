/* SPDX-License-Identifier: Proprietary */
#pragma once
/* watchdog.h — Watchdog timer interface */

#include "../../kernel/include/types.h"

int  watchdog_init(u32 timeout_ms);
void watchdog_kick(void);
void watchdog_disable(void);
