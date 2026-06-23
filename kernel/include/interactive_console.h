/* SPDX-License-Identifier: Proprietary */
#pragma once
/* AegisOS v55.1 interactive serial console.
 *
 * This is the first practical runtime console bridge: it keeps the proven
 * kernel boot chain intact, then drops the non-smoke QEMU/RPi-style serial
 * session into an Aegis shell.  The shell is kernel-hosted for v55.1 so it can
 * inspect and wire the existing Rust-oriented userland/service/RustMyAdmin
 * assets before a real Rust userspace ABI/toolchain is promoted.
 */

#include "types.h"

void interactive_console_run(void) __attribute__((noreturn));
