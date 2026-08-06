/* SPDX-License-Identifier: Proprietary */
#pragma once
/* AegisOS v44 service supervisor and fault-proof layer.
 *
 * v44 scope: build a deterministic kernel-side supervisor for the early
 * userland service set, prove fault accounting, classify a controlled
 * page-fault record, mark the affected service faulted, and preserve the
 * kernel handoff boundary before the later v45 page-table isolation work.
 */

#include "types.h"
#include "exception.h"

#define AEGIS_SUPERVISOR_SERVICE_MAX  16U
#define AEGIS_SUPERVISOR_NAME_MAX     32U

#define AEGIS_SUPERVISOR_POLICY_REQUIRED         (1U << 0)
#define AEGIS_SUPERVISOR_POLICY_RESTART_ON_FAULT (1U << 1)
#define AEGIS_SUPERVISOR_POLICY_CONTROL_PLANE    (1U << 2)

typedef enum aegis_supervised_service_state {
    AEGIS_SUPERVISED_EMPTY = 0,
    AEGIS_SUPERVISED_REGISTERED,
    AEGIS_SUPERVISED_STARTING,
    AEGIS_SUPERVISED_RUNNING,
    AEGIS_SUPERVISED_FAULTED,
    AEGIS_SUPERVISED_RESTARTING,
    AEGIS_SUPERVISED_STOPPED,
} aegis_supervised_service_state_t;

typedef enum aegis_service_fault_kind {
    AEGIS_SERVICE_FAULT_NONE = 0,
    AEGIS_SERVICE_FAULT_EXIT,
    AEGIS_SERVICE_FAULT_PAGE_FAULT,
    AEGIS_SERVICE_FAULT_WATCHDOG,
    AEGIS_SERVICE_FAULT_IPC,
} aegis_service_fault_kind_t;

typedef struct aegis_service_fault_record {
    aegis_service_fault_kind_t kind;
    u32 service_id;
    u32 pid;
    u32 fault_count_snapshot;
    s32 exit_code;
    u64 esr;
    u64 far;
    u64 elr;
    u64 ec;
    u64 fsc;
    bool lower_el;
    bool write;
    bool page_fault;
} aegis_service_fault_record_t;

typedef struct aegis_supervised_service {
    u32 id;
    u32 service_id;
    u32 pid;
    char name[AEGIS_SUPERVISOR_NAME_MAX];
    aegis_supervised_service_state_t state;
    u32 policy;
    u32 restart_count;
    u32 fault_count;
    u32 consecutive_failures;
    u32 max_restarts;
    u64 restart_window_ticks;
    u64 base_backoff_ticks;
    u64 first_failure_tick;
    u64 next_restart_tick;
    bool shutdown_requested;
    s32 last_exit_code;
    aegis_service_fault_record_t last_fault;
} aegis_supervised_service_t;

typedef struct aegis_service_supervisor_state {
    bool initialised;
    bool registry_ready;
    bool services_running;
    bool fault_proof_ready;
    bool page_fault_proof_ready;
    bool service_marked_faulted;
    bool supervisor_handoff_preserved;
    u32 service_count;
    u32 running_count;
    u32 faulted_count;
    u32 total_faults;
    u32 total_restarts;
    u32 last_fault_service_id;
    u32 last_fault_pid;
    u32 restart_throttled_count;
    u32 permanently_faulted_count;
    bool shutdown_in_progress;
    u64 last_fault_far;
    u64 last_fault_elr;
    u64 last_fault_esr;
} aegis_service_supervisor_state_t;

void service_supervisor_init(void);
int  service_supervisor_register_user_processes(void);
int  service_supervisor_start_registered(void);
int  service_supervisor_record_fault(const char *name,
                                     aegis_service_fault_kind_t kind,
                                     const aegis_exception_fault_t *fault,
                                     s32 exit_code);
int  service_supervisor_prepare_v44_fault_proof(void);
int  service_supervisor_selftest(void);
int  service_supervisor_native_register_starting(const char *name,
                                                 u32 service_id,
                                                 u32 pid,
                                                 u32 policy);
int  service_supervisor_native_mark_running(const char *name, u32 pid);
int  service_supervisor_native_mark_exit(const char *name, u32 pid, s32 exit_code);
int  service_supervisor_set_restart_policy(const char *name, u32 max_restarts,
                                           u64 restart_window_ticks,
                                           u64 base_backoff_ticks);
u64  service_supervisor_clock_ticks(void);
int  service_supervisor_restart_allowed(const char *name, u64 now_ticks, u64 *wait_ticks);
int  service_supervisor_note_restart(const char *name, u32 new_pid, u64 now_ticks);
int  service_supervisor_request_shutdown(const char *name);
void service_supervisor_begin_shutdown(void);

const aegis_service_supervisor_state_t *service_supervisor_state(void);
const aegis_supervised_service_t *service_supervisor_find(const char *name);
const aegis_supervised_service_t *service_supervisor_get(u32 id);

bool service_supervisor_registry_ready(void);
bool service_supervisor_fault_proof_ready(void);
bool service_supervisor_page_fault_proof_ready(void);
bool service_supervisor_handoff_preserved(void);
const char *service_supervisor_state_name(aegis_supervised_service_state_t state);
const char *service_supervisor_fault_kind_name(aegis_service_fault_kind_t kind);
