/* SPDX-License-Identifier: Proprietary */
#pragma once
/* aegis_kernel.h — top-level kernel include */

#include "types.h"
#include "memory.h"
#include "scheduler.h"
#include "syscalls.h"
#include "panic.h"
#include "interrupts.h"

#define AEGISOS_VERSION_MAJOR 0
#define AEGISOS_VERSION_MINOR 1
#define AEGISOS_VERSION_PATCH 0

void kernel_main(void);
