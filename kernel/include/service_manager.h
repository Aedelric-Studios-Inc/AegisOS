/* SPDX-License-Identifier: Proprietary */
#pragma once
/* AegisOS kernel service registry and early service manager.
 *
 * v28 scope: register kernel-resident services and prepare their dependency
 * graph from aegisbox-init.  Starting userland/service processes is explicitly
 * deferred to the next milestone.
 */

#include "types.h"

#define AEGIS_SERVICE_NAME_MAX 32
#define AEGIS_SERVICE_MAX      32
#define AEGIS_SERVICE_DEPS_MAX  4

typedef enum aegis_service_state {
    AEGIS_SERVICE_EMPTY = 0,
    AEGIS_SERVICE_REGISTERED,
    AEGIS_SERVICE_PREPARED,
    AEGIS_SERVICE_RUNNING,
    AEGIS_SERVICE_FAILED,
} aegis_service_state_t;

typedef enum aegis_service_flags {
    AEGIS_SERVICE_FLAG_REQUIRED = 1u << 0,
    AEGIS_SERVICE_FLAG_KERNEL   = 1u << 1,
    AEGIS_SERVICE_FLAG_USERLAND = 1u << 2,
} aegis_service_flags_t;

typedef struct aegis_service {
    u32 id;
    char name[AEGIS_SERVICE_NAME_MAX];
    aegis_service_state_t state;
    u32 flags;
    u32 dependency_count;
    u32 dependencies[AEGIS_SERVICE_DEPS_MAX];
} aegis_service_t;

void service_manager_init(void);
int  service_manager_register_kernel(const char *name,
                                     u32 flags,
                                     const char *const *dependencies,
                                     u32 dependency_count,
                                     u32 *out_id);
int  service_manager_register_builtin_kernel_services(void);
int  service_manager_prepare(void);
int  service_manager_selftest(void);

const aegis_service_t *service_manager_find(const char *name);
const aegis_service_t *service_manager_get(u32 id);
u32 service_manager_count(void);
u32 service_manager_registered_count(void);
u32 service_manager_prepared_count(void);
bool service_manager_is_prepared(void);
const char *service_state_name(aegis_service_state_t state);
