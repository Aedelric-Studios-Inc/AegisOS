/* SPDX-License-Identifier: Proprietary */
#pragma once
/* net/include/net.h — Network stack top-level interface */

#include "../../kernel/include/types.h"

int net_init(void);
void net_poll(void);
