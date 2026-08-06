/* SPDX-License-Identifier: Proprietary */
#pragma once
#include "types.h"

void entropy_init(void);
int  entropy_get(void *buf, u64 len, bool require_strong);
bool entropy_strong_ready(void);
u64  entropy_bytes_generated(void);
