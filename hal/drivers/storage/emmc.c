/* SPDX-License-Identifier: Proprietary */
/* AegisOS — hal/drivers/storage/emmc.c
 * eMMC driver stub (built on same SDHCI interface as SD).
 */

#include "../../include/storage.h"

static storage_info_t emmc_info = {
    .block_count = 0,
    .block_size  = BLOCK_SIZE,
    .label       = "emmc",
};

int emmc_init(void) {
    /* eMMC-specific initialization (OCR negotiation, ext_csd) — TODO */
    return 0;
}

int emmc_read(u32 lba, u32 count, void *buf) {
    (void)lba; (void)count; (void)buf;
    return 0;
}

int emmc_write(u32 lba, u32 count, const void *buf) {
    (void)lba; (void)count; (void)buf;
    return 0;
}

int emmc_get_info(storage_info_t *info) {
    *info = emmc_info;
    return 0;
}
