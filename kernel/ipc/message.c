/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/ipc/message.c
 * IPC message passing implementation with message queues.
 */

#include "types.h"
#include "memory.h"
#include "scheduler.h"

#define MSG_MAX_PAYLOAD 256
#define MSG_QUEUE_DEPTH  64
#define MAX_MSG_QUEUES  1024

typedef struct {
    u32  sender_tid;
    u32  receiver_tid;
    u64  tag;
    u64  payload_len;
    u8   payload[MSG_MAX_PAYLOAD];
} aegis_msg_t;

typedef struct msg_queue {
    aegis_msg_t msgs[MSG_QUEUE_DEPTH];
    u32 head;
    u32 tail;
    u32 count;
    u32 channel_id;
    bool active;
} msg_queue_t;

static msg_queue_t queues[MAX_MSG_QUEUES];

void ipc_init(void) {
    for (int i = 0; i < MAX_MSG_QUEUES; i++) {
        queues[i].active = false;
        queues[i].head = 0;
        queues[i].tail = 0;
        queues[i].count = 0;
    }
}

int ipc_create_queue(u32 channel_id) {
    for (int i = 0; i < MAX_MSG_QUEUES; i++) {
        if (!queues[i].active) {
            queues[i].active = true;
            queues[i].channel_id = channel_id;
            queues[i].head = 0;
            queues[i].tail = 0;
            queues[i].count = 0;
            return i;
        }
    }
    return AEGIS_ENOMEM;
}

int ipc_send(u32 channel_id, const void *data, u64 len) {
    if (len > MSG_MAX_PAYLOAD) return AEGIS_EINVAL;

    /* Find queue for this channel */
    msg_queue_t *q = NULL;
    for (int i = 0; i < MAX_MSG_QUEUES; i++) {
        if (queues[i].active && queues[i].channel_id == channel_id) {
            q = &queues[i];
            break;
        }
    }
    if (!q) return AEGIS_ENOENT;
    if (q->count >= MSG_QUEUE_DEPTH) return AEGIS_ENOMEM;

    /* Enqueue message */
    aegis_msg_t *msg = &q->msgs[q->tail];
    task_t *current = scheduler_current();
    msg->sender_tid = current ? current->tid : 0;
    msg->receiver_tid = 0; /* Any receiver on this channel */
    msg->tag = 0;
    msg->payload_len = len;

    const u8 *src = (const u8 *)data;
    for (u64 i = 0; i < len; i++) {
        msg->payload[i] = src[i];
    }

    q->tail = (q->tail + 1) % MSG_QUEUE_DEPTH;
    q->count++;
    return AEGIS_OK;
}

int ipc_recv(u32 channel_id, void *buf, u64 max_len) {
    /* Find queue for this channel */
    msg_queue_t *q = NULL;
    for (int i = 0; i < MAX_MSG_QUEUES; i++) {
        if (queues[i].active && queues[i].channel_id == channel_id) {
            q = &queues[i];
            break;
        }
    }
    if (!q) return AEGIS_ENOENT;

    /* Block until message available (cooperative yield) */
    while (q->count == 0) {
        scheduler_yield();
    }

    /* Dequeue message */
    aegis_msg_t *msg = &q->msgs[q->head];
    u64 copy_len = msg->payload_len < max_len ? msg->payload_len : max_len;

    u8 *dst = (u8 *)buf;
    for (u64 i = 0; i < copy_len; i++) {
        dst[i] = msg->payload[i];
    }

    q->head = (q->head + 1) % MSG_QUEUE_DEPTH;
    q->count--;
    return (int)copy_len;
}

void ipc_destroy_queue(u32 channel_id) {
    for (int i = 0; i < MAX_MSG_QUEUES; i++) {
        if (queues[i].active && queues[i].channel_id == channel_id) {
            queues[i].active = false;
            break;
        }
    }
}
