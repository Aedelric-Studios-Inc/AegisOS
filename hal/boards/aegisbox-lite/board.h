/* SPDX-License-Identifier: Proprietary */
#pragma once
/* hal/boards/aegisbox-lite/board.h */

#define BOARD_NAME        "AegisBox Lite"
#define BOARD_UART_BASE   0xFE201000UL   /* PL011 UART0 */
#define BOARD_UART_CLOCK  48000000UL
#define BOARD_UART_BAUD   115200
#define BOARD_GPIO_BASE   0xFE200000UL
#define BOARD_TIMER_FREQ  54000000UL
#define BOARD_WDT_TIMEOUT 5000          /* 5 seconds */
