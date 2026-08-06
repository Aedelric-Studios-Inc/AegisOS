/* SPDX-License-Identifier: Proprietary */
#pragma once
#include "types.h"

void system_shutdown_request(u32 requester_pid, u32 reason);
bool system_shutdown_requested(void);
u32  system_shutdown_requester(void);
u32  system_shutdown_reason(void);
void system_shutdown_commit(void) __attribute__((noreturn));
void system_reboot_commit(void) __attribute__((noreturn));
