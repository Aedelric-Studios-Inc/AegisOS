/* SPDX-License-Identifier: Proprietary */
#pragma once
/* AegisOS v50-v55 final release proof layer. */

#include "types.h"

#define AEGIS_RELEASE_VARIANT_MAX     4U
#define AEGIS_RELEASE_NAME_MAX        48U
#define AEGIS_RELEASE_SERVICE_MAX     12U
#define AEGIS_RELEASE_HARDENING_MAX   12U
#define AEGIS_RELEASE_DOC_MAX         10U

typedef struct aegis_release_variant_image {
    char name[AEGIS_RELEASE_NAME_MAX];
    char image_name[AEGIS_RELEASE_NAME_MAX];
    bool board_profile_ready;
    bool installer_profile_ready;
    bool image_declared;
    bool checksum_declared;
    bool flash_flow_hardened;
    bool hardware_notes_ready;
} aegis_release_variant_image_t;

typedef struct aegis_release_service_default {
    char name[AEGIS_RELEASE_NAME_MAX];
    bool enabled_by_default;
    bool supervised;
    bool restart_policy_frozen;
    bool persistent_config_frozen;
    bool least_privilege_profile;
} aegis_release_service_default_t;

typedef struct aegis_release_hardening_gate {
    char name[AEGIS_RELEASE_NAME_MAX];
    bool enforced;
} aegis_release_hardening_gate_t;

typedef struct aegis_release_doc_gate {
    char name[AEGIS_RELEASE_NAME_MAX];
    bool complete;
} aegis_release_doc_gate_t;

typedef struct aegis_release_final_state {
    bool initialised;
    bool v48_release_polish_inherited;
    bool v50_installer_flash_hardened;
    bool v51_variant_images_ready;
    bool v52_security_hardening_frozen;
    bool v52_service_defaults_frozen;
    bool v53_docs_ready;
    bool v53_known_limitations_ready;
    bool v53_hardware_notes_ready;
    bool v54_final_rc_audit_ready;
    bool v54_signing_manifest_ready;
    bool v54_checksums_ready;
    bool v55_release_image_ready;
    bool release_ready;
    u32 variant_count;
    u32 service_default_count;
    u32 hardening_gate_count;
    u32 doc_gate_count;
    u32 signing_key_slots;
    u32 checksum_artifacts;
    u32 manifest_generation;
    aegis_release_variant_image_t variants[AEGIS_RELEASE_VARIANT_MAX];
    aegis_release_service_default_t services[AEGIS_RELEASE_SERVICE_MAX];
    aegis_release_hardening_gate_t hardening[AEGIS_RELEASE_HARDENING_MAX];
    aegis_release_doc_gate_t docs[AEGIS_RELEASE_DOC_MAX];
} aegis_release_final_state_t;

void release_final_init(void);
int  release_final_prepare_v55(void);
int  release_final_selftest(void);

const aegis_release_final_state_t *release_final_state(void);
bool release_final_installer_flash_hardened(void);
bool release_final_variant_images_ready(void);
bool release_final_security_service_defaults_frozen(void);
bool release_final_docs_hardware_notes_ready(void);
bool release_final_rc_audit_ready(void);
bool release_final_release_image_ready(void);
bool release_final_ready(void);
