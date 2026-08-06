/* SPDX-License-Identifier: Proprietary */
/* AegisOS — hal/drivers/gpio/gpio.c
 * Generic GPIO driver.
 */

#include "../../include/gpio.h"

/* Board-specific base address is set by board init */
static uptr gpio_base;

void gpio_set_base(uptr base) {
    gpio_base = base;
}

int gpio_init(void) {
    return 0;
}

int gpio_set_direction(u32 pin, gpio_dir_t dir) {
    if (!gpio_base) return -1;
    volatile u32 *fsel = (volatile u32 *)(gpio_base + (pin / 10) * 4);
    u32 shift = (pin % 10) * 3;
    *fsel = (*fsel & ~(7U << shift)) | ((u32)dir << shift);
    return 0;
}

int gpio_write(u32 pin, gpio_val_t val) {
    if (!gpio_base) return -1;
    if (val == GPIO_HIGH) {
        *(volatile u32 *)(gpio_base + 0x1C + (pin / 32) * 4) = 1U << (pin % 32);
    } else {
        *(volatile u32 *)(gpio_base + 0x28 + (pin / 32) * 4) = 1U << (pin % 32);
    }
    return 0;
}

gpio_val_t gpio_read(u32 pin) {
    if (!gpio_base) return GPIO_LOW;
    volatile u32 *lev = (volatile u32 *)(gpio_base + 0x34 + (pin / 32) * 4);
    return (*lev >> (pin % 32)) & 1 ? GPIO_HIGH : GPIO_LOW;
}
