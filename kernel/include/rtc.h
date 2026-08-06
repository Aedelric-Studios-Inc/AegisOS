/* SPDX-License-Identifier: Proprietary */
#pragma once
#include "types.h"

void rtc_init(void);
bool rtc_ready(void);
u64  rtc_unix_seconds(void);
u64  monotonic_nanoseconds(void);
