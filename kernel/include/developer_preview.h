/* SPDX-License-Identifier: Proprietary */
#pragma once
/* AegisOS v47 AegisBox Developer Preview IMG proof layer. */

#include "types.h"

#define AEGIS_DEV_PREVIEW_VARIANT_MAX 8U
#define AEGIS_DEV_PREVIEW_NAME_MAX    32U

typedef struct aegis_dev_preview_variant {
    char name[AEGIS_DEV_PREVIEW_NAME_MAX];
    bool image_enabled;
    bool board_profile_ready;
    bool service_config_ready;
    bool installer_flow_ready;
} aegis_dev_preview_variant_t;

typedef struct aegis_dev_preview_state {
    bool initialised;
    bool manifest_ready;
    bool board_profiles_ready;
    bool installer_flash_flow_ready;
    bool service_configs_ready;
    bool security_baseline_ready;
    bool docs_ready;
    bool developer_preview_ready;
    u32 variant_count;
    u32 enabled_variants;
    u32 manifest_generation;
    u32 required_milestone_floor;
    aegis_dev_preview_variant_t variants[AEGIS_DEV_PREVIEW_VARIANT_MAX];
} aegis_dev_preview_state_t;

void developer_preview_init(void);
int  developer_preview_prepare_v47_img(void);
int  developer_preview_selftest(void);

const aegis_dev_preview_state_t *developer_preview_state(void);
bool developer_preview_ready(void);
bool developer_preview_variants_ready(void);
bool developer_preview_manifest_ready(void);
const aegis_dev_preview_variant_t *developer_preview_variant(u32 index);
