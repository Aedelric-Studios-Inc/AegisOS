/* SPDX-License-Identifier: Proprietary */
#pragma once
/* hal/boards/bastion/board.h */

#define BOARD_NAME        "Bastion"
#define BOARD_UART_BASE   0x09000000UL  /* PL011 on QEMU virt machine */
#define BOARD_UART_CLOCK  24000000UL
#define BOARD_UART_BAUD   115200
#define BOARD_TIMER_FREQ  62500000UL
#define BOARD_WDT_TIMEOUT 10000
