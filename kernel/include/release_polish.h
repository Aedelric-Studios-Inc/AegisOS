/* SPDX-License-Identifier: Proprietary */
#pragma once
/* AegisOS v48 release polish, board profile, service config, and hardening gate proof layer. */

#include "types.h"

#define AEGIS_V48_BOARD_PROFILE_MAX       8U
#define AEGIS_V48_NAME_MAX                32U
#define AEGIS_V48_SERVICE_PRESET_MAX      12U
#define AEGIS_V48_POLISH_GATE_MAX         12U

typedef struct aegis_v48_board_profile {
    char name[AEGIS_V48_NAME_MAX];
    u32 min_ram_mib;
    u32 min_storage_mib;
    u32 network_ports;
    bool router_capable;
    bool board_profile_ready;
    bool installer_profile_ready;
    bool service_config_ready;
    bool security_hardening_ready;
    bool docs_ready;
} aegis_v48_board_profile_t;

typedef struct aegis_v48_service_preset {
    char name[AEGIS_V48_NAME_MAX];
    bool enabled;
    bool supervised;
    bool restart_policy_ready;
    bool config_persisted;
    bool log_policy_ready;
} aegis_v48_service_preset_t;

typedef struct aegis_v48_polish_gate {
    char name[AEGIS_V48_NAME_MAX];
    bool passed;
} aegis_v48_polish_gate_t;

typedef struct aegis_v48_release_polish_state {
    bool initialised;
    bool v47_developer_preview_inherited;
    bool v46_network_control_inherited;
    bool board_profiles_ready;
    bool service_configs_ready;
    bool installer_flash_flow_validated;
    bool security_hardening_ready;
    bool docs_ready;
    bool image_polish_manifest_ready;
    bool release_polish_ready;
    u32 board_profile_count;
    u32 service_preset_count;
    u32 polish_gate_count;
    u32 enabled_service_presets;
    u32 router_capable_profiles;
    u32 manifest_generation;
    aegis_v48_board_profile_t board_profiles[AEGIS_V48_BOARD_PROFILE_MAX];
    aegis_v48_service_preset_t service_presets[AEGIS_V48_SERVICE_PRESET_MAX];
    aegis_v48_polish_gate_t polish_gates[AEGIS_V48_POLISH_GATE_MAX];
} aegis_v48_release_polish_state_t;

void release_polish_init(void);
int  release_polish_prepare_v48(void);
int  release_polish_selftest(void);

const aegis_v48_release_polish_state_t *release_polish_state(void);
bool release_polish_ready(void);
bool release_polish_board_profiles_ready(void);
bool release_polish_service_configs_ready(void);
bool release_polish_image_manifest_ready(void);
bool release_polish_security_hardening_ready(void);
const aegis_v48_board_profile_t *release_polish_board_profile(u32 index);
const aegis_v48_service_preset_t *release_polish_service_preset(u32 index);
