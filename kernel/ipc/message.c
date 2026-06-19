/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/ipc/message.c
 * IPC message passing implementation.
 */

#include "types.h"

#define MSG_MAX_PAYLOAD 256

typedef struct {
    u32  sender_tid;
    u32  receiver_tid;
    u64  tag;
    u64  payload_len;
    u8   payload[MSG_MAX_PAYLOAD];
} aegis_msg_t;

s64 sys_send_msg(u64 channel_id, u64 msg_ptr, u64 len, u64 a3, u64 a4, u64 a5) {
    (void)a3; (void)a4; (void)a5;
    if (len > MSG_MAX_PAYLOAD) return AEGIS_EINVAL;
    /* TODO: look up channel, copy payload, wake receiver */
    (void)channel_id; (void)msg_ptr;
    return AEGIS_OK;
}

s64 sys_recv_msg(u64 channel_id, u64 buf_ptr, u64 buf_len, u64 a3, u64 a4, u64 a5) {
    (void)a3; (void)a4; (void)a5;
    (void)channel_id; (void)buf_ptr; (void)buf_len;
    /* TODO: block until message available */
    return AEGIS_OK;
}
