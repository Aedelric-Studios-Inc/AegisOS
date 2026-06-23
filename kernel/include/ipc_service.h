/* SPDX-License-Identifier: Proprietary */
#pragma once
/* AegisOS v43 service IPC contract.
 *
 * This layer sits above the low-level channel/message queue primitives and
 * gives the early userland service set a deterministic mailbox topology.
 * v43 proves that services can be given kernel-owned mailbox channels and
 * that the expanded SYS_SEND_MSG/SYS_RECV_MSG surface can perform a safe
 * roundtrip before true service scheduling lands.
 */

#include "types.h"

#define AEGIS_IPC_SERVICE_NAME_MAX 32
#define AEGIS_IPC_MAILBOX_MAX       8
#define AEGIS_IPC_TEST_PAYLOAD_MAX  64

typedef enum aegis_ipc_mailbox_state {
    AEGIS_IPC_MAILBOX_EMPTY = 0,
    AEGIS_IPC_MAILBOX_REGISTERED,
    AEGIS_IPC_MAILBOX_CHANNEL_READY,
    AEGIS_IPC_MAILBOX_ROUNDTRIP_PROVED,
} aegis_ipc_mailbox_state_t;

typedef struct aegis_service_mailbox {
    u32 id;
    u32 owner_pid;
    u32 peer_pid;
    u32 service_id;
    u32 channel_id;
    u32 queue_id;
    char service_name[AEGIS_IPC_SERVICE_NAME_MAX];
    char peer_name[AEGIS_IPC_SERVICE_NAME_MAX];
    aegis_ipc_mailbox_state_t state;
    u32 sent_count;
    u32 recv_count;
    u32 last_payload_len;
} aegis_service_mailbox_t;

typedef struct aegis_ipc_service_state {
    bool initialised;
    bool service_messaging_ready;
    bool message_roundtrip_ready;
    u32 mailbox_count;
    u32 channel_count;
    u32 route_count;
    u32 last_channel_id;
    u32 last_roundtrip_bytes;
} aegis_ipc_service_state_t;

void service_ipc_init(void);
int  service_ipc_prepare_service_messaging(void);
int  service_ipc_service_messaging_selftest(void);
int  service_ipc_message_roundtrip_selftest(void);
bool service_ipc_service_messaging_ready(void);
bool service_ipc_message_roundtrip_ready(void);
const aegis_ipc_service_state_t *service_ipc_state(void);
const aegis_service_mailbox_t *service_ipc_mailbox(u32 index);
const char *service_ipc_mailbox_state_name(aegis_ipc_mailbox_state_t state);
