/* SPDX-License-Identifier: Proprietary */
#pragma once
#include "types.h"

typedef struct aegis_install_result {
    u32 source_device;
    u32 target_device;
    u64 sectors_copied;
    u64 sectors_verified;
    u32 crc_source;
    u32 crc_target;
    int status;
} aegis_install_result_t;

int aegis_installer_install_to_nvme(aegis_install_result_t *result);
const aegis_install_result_t *aegis_installer_last_result(void);
