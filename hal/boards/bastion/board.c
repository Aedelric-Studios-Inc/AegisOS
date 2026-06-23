/* SPDX-License-Identifier: Proprietary */
/* AegisOS — hal/boards/bastion/board.c */

#include "board.h"
#include "../../include/uart.h"
#include "../../include/timer.h"
#include "../../include/watchdog.h"

int board_init(void) {
    const uart_config_t uart_cfg = {
        .base_addr = BOARD_UART_BASE,
        .baud_rate = BOARD_UART_BAUD,
        .clock_hz  = BOARD_UART_CLOCK,
    };
    uart_init(&uart_cfg);
    timer_init(BOARD_TIMER_FREQ);
    watchdog_init(BOARD_WDT_TIMEOUT);
    return 0;
}
