/* SPDX-License-Identifier: Proprietary */
#pragma once
/* interrupts.h — global EL1 interrupt control */

#include "types.h"

void irq_global_disable(void);
bool irq_global_is_enabled(void);
bool irq_try_global_enable(void);

void irq_enable_set_allowed(bool allowed);
bool irq_enable_is_allowed(void);
