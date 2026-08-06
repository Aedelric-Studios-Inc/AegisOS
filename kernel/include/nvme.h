/* SPDX-License-Identifier: Proprietary */
#pragma once
#include "types.h"
int nvme_init(void);
bool nvme_ready(void);
u64 nvme_capacity_sectors(void);
