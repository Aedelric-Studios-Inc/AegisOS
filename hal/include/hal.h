/* SPDX-License-Identifier: Proprietary */
#pragma once
/* hal.h — Hardware Abstraction Layer top-level interface */

#include "../../kernel/include/types.h"

int  hal_init(void);
void hal_reset(void);
void hal_poweroff(void);
u32  hal_get_cpu_id(void);
u64  hal_get_cpu_freq(void);
