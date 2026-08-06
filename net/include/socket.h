/* SPDX-License-Identifier: Proprietary */
#pragma once
#include "../../kernel/include/types.h"

#define AEGIS_SOCK_STREAM 1U
#define AEGIS_SOCK_DGRAM  2U
#define AEGIS_SOCKET_MAX  64U
#define AEGIS_SOCKET_FD_BASE 0x1000U

void net_socket_init(void);
int  net_socket_create(u32 owner_pid, u32 type);
int  net_socket_close(u32 owner_pid, int fd);
void net_socket_close_all(u32 owner_pid);
int  net_socket_bind(u32 owner_pid, int fd, u16 port);
int  net_socket_listen(u32 owner_pid, int fd);
int  net_socket_accept(u32 owner_pid, int fd);
int  net_socket_connect(u32 owner_pid, int fd, u32 ip, u16 port);
int  net_socket_send(u32 owner_pid, int fd, const void *data, u32 len);
int  net_socket_recv(u32 owner_pid, int fd, void *data, u32 len);
void network_poll(void);
u32  net_socket_listening_count(void);
u32  net_socket_connected_count(void);
int  net_socket_selftest(void);
