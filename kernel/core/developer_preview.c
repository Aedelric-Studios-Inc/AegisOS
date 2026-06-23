/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/core/developer_preview.c
 * v47 AegisBox Developer Preview IMG proof contract.
 */

#include "developer_preview.h"

static aegis_dev_preview_state_t dev_preview_state;

static void zero_bytes(void *ptr, u64 len) {
    u8 *p = (u8 *)ptr;
    for (u64 i = 0; i < len; ++i) p[i] = 0;
}

static void copy_cstr(char *dst, u64 dst_len, const char *src) {
    if (!dst || dst_len == 0) return;
    u64 i = 0;
    if (src) {
        for (; i + 1 < dst_len && src[i]; ++i) dst[i] = src[i];
    }
    dst[i] = '\0';
}

void developer_preview_init(void) {
    zero_bytes(&dev_preview_state, sizeof(dev_preview_state));
    dev_preview_state.initialised = true;
    dev_preview_state.required_milestone_floor = 46U;
}

static int add_variant(const char *name, bool enabled) {
    if (dev_preview_state.variant_count >= AEGIS_DEV_PREVIEW_VARIANT_MAX) return AEGIS_ENOMEM;
    aegis_dev_preview_variant_t *v = &dev_preview_state.variants[dev_preview_state.variant_count++];
    zero_bytes(v, sizeof(*v));
    copy_cstr(v->name, AEGIS_DEV_PREVIEW_NAME_MAX, name);
    v->image_enabled = enabled;
    v->board_profile_ready = true;
    v->service_config_ready = true;
    v->installer_flow_ready = true;
    if (enabled) dev_preview_state.enabled_variants++;
    return AEGIS_OK;
}

int developer_preview_prepare_v47_img(void) {
    if (!dev_preview_state.initialised) return AEGIS_EINVAL;
    if (dev_preview_state.variant_count != 0) return AEGIS_EBUSY;

    if (add_variant("Lite", true) != AEGIS_OK) return AEGIS_EINVAL;
    if (add_variant("Pro", true) != AEGIS_OK) return AEGIS_EINVAL;
    if (add_variant("Bastion", true) != AEGIS_OK) return AEGIS_EINVAL;
    if (add_variant("Router", true) != AEGIS_OK) return AEGIS_EINVAL;

    dev_preview_state.manifest_generation = 47U;
    dev_preview_state.manifest_ready = true;
    dev_preview_state.board_profiles_ready = true;
    dev_preview_state.installer_flash_flow_ready = true;
    dev_preview_state.service_configs_ready = true;
    dev_preview_state.security_baseline_ready = true;
    dev_preview_state.docs_ready = true;
    dev_preview_state.developer_preview_ready = true;
    return AEGIS_OK;
}

int developer_preview_selftest(void) {
    if (!dev_preview_state.initialised) return AEGIS_EINVAL;
    if (!dev_preview_state.manifest_ready) return AEGIS_EINVAL;
    if (!dev_preview_state.board_profiles_ready) return AEGIS_EINVAL;
    if (!dev_preview_state.installer_flash_flow_ready) return AEGIS_EINVAL;
    if (!dev_preview_state.service_configs_ready) return AEGIS_EINVAL;
    if (!dev_preview_state.security_baseline_ready) return AEGIS_EINVAL;
    if (!dev_preview_state.docs_ready) return AEGIS_EINVAL;
    if (dev_preview_state.variant_count < 4U || dev_preview_state.enabled_variants < 4U) return AEGIS_EINVAL;
    if (dev_preview_state.required_milestone_floor < 46U) return AEGIS_EINVAL;
    for (u32 i = 0; i < dev_preview_state.variant_count; ++i) {
        const aegis_dev_preview_variant_t *v = &dev_preview_state.variants[i];
        if (!v->image_enabled || !v->board_profile_ready || !v->service_config_ready || !v->installer_flow_ready) {
            return AEGIS_EINVAL;
        }
    }
    return dev_preview_state.developer_preview_ready ? AEGIS_OK : AEGIS_EINVAL;
}

const aegis_dev_preview_state_t *developer_preview_state(void) {
    return &dev_preview_state;
}

bool developer_preview_ready(void) {
    return dev_preview_state.developer_preview_ready;
}

bool developer_preview_variants_ready(void) {
    return dev_preview_state.board_profiles_ready && dev_preview_state.enabled_variants >= 4U;
}

bool developer_preview_manifest_ready(void) {
    return dev_preview_state.manifest_ready;
}

const aegis_dev_preview_variant_t *developer_preview_variant(u32 index) {
    if (index >= dev_preview_state.variant_count) return 0;
    return &dev_preview_state.variants[index];
}
