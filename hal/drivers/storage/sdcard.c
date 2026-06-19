/* SPDX-License-Identifier: Proprietary */
/* AegisOS — hal/drivers/storage/sdcard.c
 * SD card driver (SDIO/SDHCI interface stub).
 */

#include "../../include/storage.h"

static storage_info_t sdcard_info = {
    .block_count = 0,
    .block_size  = BLOCK_SIZE,
    .label       = "sdcard",
};

int storage_init(void) {
    /* SDHCI controller init — TODO */
    sdcard_info.block_count = 0;
    return 0;
}

int storage_read(u32 lba, u32 count, void *buf) {
    if (!buf || count == 0) return -1;
    /* Issue READ_MULTIPLE_BLOCK command — TODO */
    (void)lba;
    return 0;
}

int storage_write(u32 lba, u32 count, const void *buf) {
    if (!buf || count == 0) return -1;
    /* Issue WRITE_MULTIPLE_BLOCK command — TODO */
    (void)lba;
    return 0;
}

int storage_get_info(storage_info_t *info) {
    *info = sdcard_info;
    return 0;
}
