/* SPDX-License-Identifier: Proprietary */
#pragma once
/* gpio.h — GPIO driver interface */

#include "../../kernel/include/types.h"

typedef enum { GPIO_INPUT = 0, GPIO_OUTPUT = 1, GPIO_ALT = 2 } gpio_dir_t;
typedef enum { GPIO_LOW = 0, GPIO_HIGH = 1 } gpio_val_t;

int      gpio_init(void);
int      gpio_set_direction(u32 pin, gpio_dir_t dir);
int      gpio_write(u32 pin, gpio_val_t val);
gpio_val_t gpio_read(u32 pin);
