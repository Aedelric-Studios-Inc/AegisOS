/* SPDX-License-Identifier: Proprietary */
/* AegisOS — hal/drivers/watchdog/watchdog.c
 * Hardware watchdog timer driver.
 */

#include "../../include/watchdog.h"

/* SP805 watchdog registers */
#define WDT_BASE    0x09010000UL
#define WDT_LOAD    (*(volatile u32 *)(WDT_BASE + 0x000))
#define WDT_CONTROL (*(volatile u32 *)(WDT_BASE + 0x008))
#define WDT_INTCLR  (*(volatile u32 *)(WDT_BASE + 0x00C))
#define WDT_LOCK    (*(volatile u32 *)(WDT_BASE + 0xC00))

#define WDT_UNLOCK_VAL 0x1ACCE551
#define WDT_RESEN      (1 << 1)
#define WDT_INTEN      (1 << 0)

static u32 wdt_load_val;

int watchdog_init(u32 timeout_ms) {
    /* Assuming 1 MHz WDT clock */
    wdt_load_val  = timeout_ms * 1000;
    WDT_LOCK    = WDT_UNLOCK_VAL;
    WDT_LOAD    = wdt_load_val;
    WDT_CONTROL = WDT_RESEN | WDT_INTEN;
    WDT_LOCK    = 0;
    return 0;
}

void watchdog_kick(void) {
    WDT_LOCK   = WDT_UNLOCK_VAL;
    WDT_INTCLR = 1;
    WDT_LOAD   = wdt_load_val;
    WDT_LOCK   = 0;
}

void watchdog_disable(void) {
    WDT_LOCK    = WDT_UNLOCK_VAL;
    WDT_CONTROL = 0;
    WDT_LOCK    = 0;
}
