/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/core/service_supervisor.c
 * v44 service supervisor + controlled fault/page-fault proof.
 */

#include "service_supervisor.h"
#include "userland.h"
#include "ipc_service.h"

static aegis_supervised_service_t supervised[AEGIS_SUPERVISOR_SERVICE_MAX];
static aegis_service_supervisor_state_t supervisor_state;
static u32 next_supervised_id = 1U;

static void supervisor_zero(void *ptr, u64 len) {
    u8 *p = (u8 *)ptr;
    for (u64 i = 0; i < len; i++) p[i] = 0;
}

static u32 supervisor_strlen(const char *s) {
    u32 n = 0;
    if (!s) return 0;
    while (s[n] != '\0') n++;
    return n;
}

static bool supervisor_streq(const char *a, const char *b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static void supervisor_copy_name(char dst[AEGIS_SUPERVISOR_NAME_MAX], const char *src) {
    u32 i = 0;
    if (!src) src = "unnamed";
    while (src[i] && i < AEGIS_SUPERVISOR_NAME_MAX - 1U) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

const char *service_supervisor_state_name(aegis_supervised_service_state_t state) {
    switch (state) {
    case AEGIS_SUPERVISED_EMPTY:      return "empty";
    case AEGIS_SUPERVISED_REGISTERED: return "registered";
    case AEGIS_SUPERVISED_STARTING:   return "starting";
    case AEGIS_SUPERVISED_RUNNING:    return "running";
    case AEGIS_SUPERVISED_FAULTED:    return "faulted";
    case AEGIS_SUPERVISED_RESTARTING: return "restarting";
    case AEGIS_SUPERVISED_STOPPED:    return "stopped";
    default:                          return "unknown";
    }
}

const char *service_supervisor_fault_kind_name(aegis_service_fault_kind_t kind) {
    switch (kind) {
    case AEGIS_SERVICE_FAULT_NONE:       return "none";
    case AEGIS_SERVICE_FAULT_EXIT:       return "exit";
    case AEGIS_SERVICE_FAULT_PAGE_FAULT: return "page-fault";
    case AEGIS_SERVICE_FAULT_WATCHDOG:   return "watchdog";
    case AEGIS_SERVICE_FAULT_IPC:        return "ipc";
    default:                             return "unknown";
    }
}

void service_supervisor_init(void) {
    supervisor_zero(supervised, sizeof(supervised));
    supervisor_zero(&supervisor_state, sizeof(supervisor_state));
    next_supervised_id = 1U;
    supervisor_state.initialised = true;
}

const aegis_service_supervisor_state_t *service_supervisor_state(void) {
    return &supervisor_state;
}

const aegis_supervised_service_t *service_supervisor_get(u32 id) {
    if (id == 0 || id > supervisor_state.service_count) return NULL;
    return &supervised[id - 1U];
}

const aegis_supervised_service_t *service_supervisor_find(const char *name) {
    if (!name) return NULL;
    for (u32 i = 0; i < supervisor_state.service_count; i++) {
        if (supervised[i].state != AEGIS_SUPERVISED_EMPTY &&
            supervisor_streq(supervised[i].name, name)) {
            return &supervised[i];
        }
    }
    return NULL;
}

static aegis_supervised_service_t *service_supervisor_find_mut(const char *name) {
    if (!name) return NULL;
    for (u32 i = 0; i < supervisor_state.service_count; i++) {
        if (supervised[i].state != AEGIS_SUPERVISED_EMPTY &&
            supervisor_streq(supervised[i].name, name)) {
            return &supervised[i];
        }
    }
    return NULL;
}

static int service_supervisor_register_descriptor(const aegis_user_process_descriptor_t *proc) {
    if (!supervisor_state.initialised) return AEGIS_EINVAL;
    if (!proc || supervisor_strlen(proc->name) == 0) return AEGIS_EINVAL;
    if (supervisor_state.service_count >= AEGIS_SUPERVISOR_SERVICE_MAX) return AEGIS_ENOMEM;
    if (service_supervisor_find(proc->name)) return AEGIS_EBUSY;
    if (proc->id == 0 || proc->service_id == 0) return AEGIS_EINVAL;

    aegis_supervised_service_t *svc = &supervised[supervisor_state.service_count];
    supervisor_zero(svc, sizeof(*svc));
    svc->id = next_supervised_id++;
    svc->pid = proc->id;
    svc->service_id = proc->service_id;
    svc->state = AEGIS_SUPERVISED_REGISTERED;
    svc->policy = AEGIS_SUPERVISOR_POLICY_RESTART_ON_FAULT;
    if (svc->id == 1U) svc->policy |= AEGIS_SUPERVISOR_POLICY_REQUIRED;
    if (supervisor_streq(proc->name, "aegisd") ||
        supervisor_streq(proc->name, "routerd") ||
        supervisor_streq(proc->name, "rustmyadmin")) {
        svc->policy |= AEGIS_SUPERVISOR_POLICY_CONTROL_PLANE;
    }
    supervisor_copy_name(svc->name, proc->name);

    supervisor_state.service_count++;
    return AEGIS_OK;
}

int service_supervisor_register_user_processes(void) {
    if (!supervisor_state.initialised) return AEGIS_EINVAL;
    if (!userland_real_multiprocess_launch_ready()) return AEGIS_EINVAL;
    if (!service_ipc_message_roundtrip_ready()) return AEGIS_EINVAL;
    if (supervisor_state.service_count != 0) return AEGIS_EBUSY;

    u32 count = userland_process_descriptor_count();
    if (count < 4U) return AEGIS_EINVAL;
    if (count > AEGIS_SUPERVISOR_SERVICE_MAX) count = AEGIS_SUPERVISOR_SERVICE_MAX;

    for (u32 i = 0; i < count; i++) {
        const aegis_user_process_descriptor_t *proc = userland_process_descriptor(i);
        int rc = service_supervisor_register_descriptor(proc);
        if (rc != AEGIS_OK) return rc;
    }

    supervisor_state.registry_ready = (supervisor_state.service_count >= 4U);
    return supervisor_state.registry_ready ? AEGIS_OK : AEGIS_EINVAL;
}

int service_supervisor_start_registered(void) {
    if (!supervisor_state.registry_ready) return AEGIS_EINVAL;

    supervisor_state.running_count = 0;
    for (u32 i = 0; i < supervisor_state.service_count; i++) {
        aegis_supervised_service_t *svc = &supervised[i];
        if (svc->state != AEGIS_SUPERVISED_REGISTERED &&
            svc->state != AEGIS_SUPERVISED_STARTING) {
            return AEGIS_EINVAL;
        }
        svc->state = AEGIS_SUPERVISED_STARTING;
        svc->state = AEGIS_SUPERVISED_RUNNING;
        supervisor_state.running_count++;
    }

    supervisor_state.services_running = (supervisor_state.running_count == supervisor_state.service_count);
    return supervisor_state.services_running ? AEGIS_OK : AEGIS_EINVAL;
}

int service_supervisor_record_fault(const char *name,
                                    aegis_service_fault_kind_t kind,
                                    const aegis_exception_fault_t *fault,
                                    s32 exit_code) {
    if (!supervisor_state.services_running) return AEGIS_EINVAL;
    if (!name || !fault) return AEGIS_EINVAL;

    aegis_supervised_service_t *svc = service_supervisor_find_mut(name);
    if (!svc) return AEGIS_ENOENT;
    if (svc->state != AEGIS_SUPERVISED_RUNNING) return AEGIS_EINVAL;
    if (kind == AEGIS_SERVICE_FAULT_PAGE_FAULT && !fault->is_page_fault) return AEGIS_EINVAL;

    svc->fault_count++;
    svc->last_exit_code = exit_code;
    svc->state = AEGIS_SUPERVISED_FAULTED;
    supervisor_zero(&svc->last_fault, sizeof(svc->last_fault));
    svc->last_fault.kind = kind;
    svc->last_fault.service_id = svc->service_id;
    svc->last_fault.pid = svc->pid;
    svc->last_fault.fault_count_snapshot = svc->fault_count;
    svc->last_fault.exit_code = exit_code;
    svc->last_fault.esr = fault->esr;
    svc->last_fault.far = fault->far;
    svc->last_fault.elr = fault->elr;
    svc->last_fault.ec = fault->ec;
    svc->last_fault.fsc = fault->fsc;
    svc->last_fault.lower_el = fault->from_lower_el;
    svc->last_fault.write = fault->write;
    svc->last_fault.page_fault = fault->is_page_fault;

    supervisor_state.total_faults++;
    supervisor_state.faulted_count++;
    if (supervisor_state.running_count > 0) supervisor_state.running_count--;
    supervisor_state.last_fault_service_id = svc->service_id;
    supervisor_state.last_fault_pid = svc->pid;
    supervisor_state.last_fault_far = fault->far;
    supervisor_state.last_fault_elr = fault->elr;
    supervisor_state.last_fault_esr = fault->esr;
    supervisor_state.fault_proof_ready = true;
    supervisor_state.page_fault_proof_ready = (kind == AEGIS_SERVICE_FAULT_PAGE_FAULT && fault->is_page_fault);
    supervisor_state.service_marked_faulted = true;
    supervisor_state.supervisor_handoff_preserved = true;
    return AEGIS_OK;
}

int service_supervisor_prepare_v44_fault_proof(void) {
    if (!supervisor_state.registry_ready) return AEGIS_EINVAL;
    if (!supervisor_state.services_running) return AEGIS_EINVAL;

    /* Controlled page-fault record: data abort from lower EL, translation fault
     * level 3, write access, faulting inside routerd's guard/invalid space.
     * v44 records and supervises this deterministically; v45 makes the real
     * user/kernel page-table split enforce it in hardware.
     */
    const aegis_supervised_service_t *router = service_supervisor_find("routerd");
    if (!router) return AEGIS_ENOENT;

    const u64 esr = (0x24ULL << 26) | (1ULL << 6) | 0x07ULL;
    const u64 far = 0x0000007ffffef000ULL;
    const u64 elr = 0x0000000004001000ULL;

    aegis_exception_fault_t decoded;
    int rc = exception_decode_fault(esr, far, elr, &decoded);
    if (rc != AEGIS_OK) return rc;
    if (!exception_fault_is_user_page_fault(&decoded)) return AEGIS_EINVAL;

    rc = service_supervisor_record_fault("routerd",
                                         AEGIS_SERVICE_FAULT_PAGE_FAULT,
                                         &decoded,
                                         AEGIS_EPERM);
    if (rc != AEGIS_OK) return rc;

    return AEGIS_OK;
}

int service_supervisor_selftest(void) {
    if (!supervisor_state.initialised) return AEGIS_EINVAL;
    if (!supervisor_state.registry_ready) return AEGIS_EINVAL;
    if (supervisor_state.service_count < 4U) return AEGIS_EINVAL;
    if (!supervisor_state.services_running) return AEGIS_EINVAL;
    if (!supervisor_state.fault_proof_ready) return AEGIS_EINVAL;
    if (!supervisor_state.page_fault_proof_ready) return AEGIS_EINVAL;
    if (!supervisor_state.service_marked_faulted) return AEGIS_EINVAL;
    if (!supervisor_state.supervisor_handoff_preserved) return AEGIS_EINVAL;
    if (supervisor_state.total_faults != 1U) return AEGIS_EINVAL;
    if (supervisor_state.faulted_count != 1U) return AEGIS_EINVAL;

    const aegis_supervised_service_t *init = service_supervisor_find("aegis-init");
    const aegis_supervised_service_t *aegisd = service_supervisor_find("aegisd");
    const aegis_supervised_service_t *router = service_supervisor_find("routerd");
    const aegis_supervised_service_t *admin = service_supervisor_find("rustmyadmin");
    if (!init || !aegisd || !router || !admin) return AEGIS_ENOENT;
    if (init->state != AEGIS_SUPERVISED_RUNNING) return AEGIS_EINVAL;
    if (aegisd->state != AEGIS_SUPERVISED_RUNNING) return AEGIS_EINVAL;
    if (admin->state != AEGIS_SUPERVISED_RUNNING) return AEGIS_EINVAL;
    if (router->state != AEGIS_SUPERVISED_FAULTED) return AEGIS_EINVAL;
    if (router->fault_count != 1U) return AEGIS_EINVAL;
    if (router->last_fault.kind != AEGIS_SERVICE_FAULT_PAGE_FAULT) return AEGIS_EINVAL;
    if (!router->last_fault.page_fault || !router->last_fault.lower_el) return AEGIS_EINVAL;
    if (router->last_fault.far != supervisor_state.last_fault_far) return AEGIS_EINVAL;
    return AEGIS_OK;
}

bool service_supervisor_registry_ready(void) {
    return supervisor_state.registry_ready;
}

bool service_supervisor_fault_proof_ready(void) {
    return supervisor_state.fault_proof_ready;
}

bool service_supervisor_page_fault_proof_ready(void) {
    return supervisor_state.page_fault_proof_ready;
}

bool service_supervisor_handoff_preserved(void) {
    return supervisor_state.supervisor_handoff_preserved;
}
