/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/core/service_manager.c */

#include "service_manager.h"
#include "panic.h"

static aegis_service_t services[AEGIS_SERVICE_MAX];
static u32 service_count;
static bool manager_prepared;

static void service_zero(void *ptr, u64 size) {
    u8 *p = (u8 *)ptr;
    for (u64 i = 0; i < size; i++) p[i] = 0;
}

static u32 str_len(const char *s) {
    u32 n = 0;
    if (!s) return 0;
    while (s[n] != '\0') n++;
    return n;
}

static bool str_eq(const char *a, const char *b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static void name_copy(char dst[AEGIS_SERVICE_NAME_MAX], const char *src) {
    u32 i = 0;
    if (!src) src = "unnamed";
    while (src[i] && i < AEGIS_SERVICE_NAME_MAX - 1U) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

const char *service_state_name(aegis_service_state_t state) {
    switch (state) {
    case AEGIS_SERVICE_EMPTY:      return "empty";
    case AEGIS_SERVICE_REGISTERED: return "registered";
    case AEGIS_SERVICE_PREPARED:   return "prepared";
    case AEGIS_SERVICE_RUNNING:    return "running";
    case AEGIS_SERVICE_FAILED:     return "failed";
    default:                       return "unknown";
    }
}

void service_manager_init(void) {
    service_zero(services, sizeof(services));
    service_count = 0;
    manager_prepared = false;
}

const aegis_service_t *service_manager_get(u32 id) {
    if (id == 0 || id > service_count) return NULL;
    return &services[id - 1U];
}

const aegis_service_t *service_manager_find(const char *name) {
    if (!name) return NULL;
    for (u32 i = 0; i < service_count; i++) {
        if (services[i].state != AEGIS_SERVICE_EMPTY && str_eq(services[i].name, name)) {
            return &services[i];
        }
    }
    return NULL;
}

int service_manager_register_kernel(const char *name,
                                    u32 flags,
                                    const char *const *dependencies,
                                    u32 dependency_count,
                                    u32 *out_id) {
    if (!name || str_len(name) == 0 || dependency_count > AEGIS_SERVICE_DEPS_MAX) {
        return AEGIS_EINVAL;
    }
    if (manager_prepared) return AEGIS_EBUSY;
    if (service_count >= AEGIS_SERVICE_MAX) return AEGIS_ENOMEM;
    if (service_manager_find(name)) return AEGIS_EBUSY;

    aegis_service_t *svc = &services[service_count];
    service_zero(svc, sizeof(*svc));
    svc->id = service_count + 1U;
    svc->state = AEGIS_SERVICE_REGISTERED;
    svc->flags = flags | AEGIS_SERVICE_FLAG_KERNEL;
    svc->dependency_count = dependency_count;
    name_copy(svc->name, name);

    for (u32 i = 0; i < dependency_count; i++) {
        if (!dependencies || !dependencies[i]) return AEGIS_EINVAL;
        const aegis_service_t *dep = service_manager_find(dependencies[i]);
        if (!dep) return AEGIS_ENOENT;
        svc->dependencies[i] = dep->id;
    }

    service_count++;
    if (out_id) *out_id = svc->id;
    return AEGIS_OK;
}

int service_manager_register_builtin_kernel_services(void) {
    u32 id = 0;
    const char *const initramfs_deps[] = { "vfs" };
    const char *const devfs_deps[]     = { "vfs" };
    const char *const procfs_deps[]    = { "vfs" };
    const char *const ipc_deps[]       = { "capability" };
    const char *const security_deps[]  = { "capability", "ipc" };
    const char *const router_deps[]    = { "network", "firewall", "security" };
    const char *const aegisd_deps[]    = { "vfs", "ipc", "security", "router" };

    int rc;
    rc = service_manager_register_kernel("vfs", AEGIS_SERVICE_FLAG_REQUIRED, NULL, 0, &id);
    if (rc != AEGIS_OK) return rc;
    rc = service_manager_register_kernel("initramfs", AEGIS_SERVICE_FLAG_REQUIRED, initramfs_deps, 1, &id);
    if (rc != AEGIS_OK) return rc;
    rc = service_manager_register_kernel("devfs", AEGIS_SERVICE_FLAG_REQUIRED, devfs_deps, 1, &id);
    if (rc != AEGIS_OK) return rc;
    rc = service_manager_register_kernel("procfs", AEGIS_SERVICE_FLAG_REQUIRED, procfs_deps, 1, &id);
    if (rc != AEGIS_OK) return rc;
    rc = service_manager_register_kernel("capability", AEGIS_SERVICE_FLAG_REQUIRED, NULL, 0, &id);
    if (rc != AEGIS_OK) return rc;
    rc = service_manager_register_kernel("ipc", AEGIS_SERVICE_FLAG_REQUIRED, ipc_deps, 1, &id);
    if (rc != AEGIS_OK) return rc;
    rc = service_manager_register_kernel("security", AEGIS_SERVICE_FLAG_REQUIRED, security_deps, 2, &id);
    if (rc != AEGIS_OK) return rc;
    rc = service_manager_register_kernel("network", AEGIS_SERVICE_FLAG_REQUIRED, NULL, 0, &id);
    if (rc != AEGIS_OK) return rc;
    rc = service_manager_register_kernel("firewall", AEGIS_SERVICE_FLAG_REQUIRED, NULL, 0, &id);
    if (rc != AEGIS_OK) return rc;
    rc = service_manager_register_kernel("router", AEGIS_SERVICE_FLAG_REQUIRED, router_deps, 3, &id);
    if (rc != AEGIS_OK) return rc;
    rc = service_manager_register_kernel("aegisd", AEGIS_SERVICE_FLAG_REQUIRED, aegisd_deps, 4, &id);
    if (rc != AEGIS_OK) return rc;

    return AEGIS_OK;
}

static bool dependency_is_prepared(const aegis_service_t *svc, u32 dep_id) {
    (void)svc;
    const aegis_service_t *dep = service_manager_get(dep_id);
    return dep && dep->state == AEGIS_SERVICE_PREPARED;
}

int service_manager_prepare(void) {
    if (service_count == 0) return AEGIS_EINVAL;

    u32 prepared_total = 0;
    for (u32 pass = 0; pass < AEGIS_SERVICE_MAX && prepared_total < service_count; pass++) {
        bool progressed = false;
        for (u32 i = 0; i < service_count; i++) {
            aegis_service_t *svc = &services[i];
            if (svc->state == AEGIS_SERVICE_PREPARED) continue;
            if (svc->state != AEGIS_SERVICE_REGISTERED) return AEGIS_EINVAL;

            bool deps_ok = true;
            for (u32 d = 0; d < svc->dependency_count; d++) {
                if (!dependency_is_prepared(svc, svc->dependencies[d])) {
                    deps_ok = false;
                    break;
                }
            }
            if (!deps_ok) continue;

            svc->state = AEGIS_SERVICE_PREPARED;
            prepared_total++;
            progressed = true;
        }
        if (!progressed) break;
    }

    if (prepared_total != service_count) {
        return AEGIS_EINVAL;
    }

    manager_prepared = true;
    return AEGIS_OK;
}

int service_manager_selftest(void) {
    if (!manager_prepared) return AEGIS_EINVAL;
    if (service_count < 10U) return AEGIS_EINVAL;
    if (service_manager_prepared_count() != service_count) return AEGIS_EINVAL;

    const aegis_service_t *vfs = service_manager_find("vfs");
    const aegis_service_t *initramfs = service_manager_find("initramfs");
    const aegis_service_t *aegisd = service_manager_find("aegisd");
    const aegis_service_t *router = service_manager_find("router");
    if (!vfs || !initramfs || !aegisd || !router) return AEGIS_ENOENT;
    if (vfs->state != AEGIS_SERVICE_PREPARED) return AEGIS_EINVAL;
    if (initramfs->dependency_count != 1U || initramfs->dependencies[0] != vfs->id) return AEGIS_EINVAL;
    if (router->dependency_count != 3U) return AEGIS_EINVAL;
    if (aegisd->dependency_count != 4U) return AEGIS_EINVAL;
    if ((aegisd->flags & AEGIS_SERVICE_FLAG_KERNEL) == 0) return AEGIS_EINVAL;
    return AEGIS_OK;
}

u32 service_manager_count(void) {
    return service_count;
}

u32 service_manager_registered_count(void) {
    u32 count = 0;
    for (u32 i = 0; i < service_count; i++) {
        if (services[i].state == AEGIS_SERVICE_REGISTERED) count++;
    }
    return count;
}

u32 service_manager_prepared_count(void) {
    u32 count = 0;
    for (u32 i = 0; i < service_count; i++) {
        if (services[i].state == AEGIS_SERVICE_PREPARED) count++;
    }
    return count;
}

bool service_manager_is_prepared(void) {
    return manager_prepared;
}
