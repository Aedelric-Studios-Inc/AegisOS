/* SPDX-License-Identifier: Proprietary */
#include "include/socket.h"
#include "include/packet.h"
#include "../hal/include/ethernet.h"

extern void ethernet_rx(pktbuf_t *pkt);
extern int udp_bind(u16 port);
extern void udp_unbind(int sock_id);
extern int udp_send(u32 dst_ip,u16 dst_port,u16 src_port,const u8*data,u16 len);
extern int udp_recv(int sock_id,u8*buf,u16 max_len,u32*src_ip,u16*src_port);
extern int tcp_listen(u16 port);
extern int tcp_connect(u32 remote_ip,u16 remote_port);
extern int tcp_accept(int listener_id);
extern int tcp_recv_id(int conn_id,u8*buf,u16 max_len);
extern int tcp_send_id(int conn_id,const u8*data,u16 len);
extern int tcp_close_id(int conn_id);
extern bool tcp_is_established(int conn_id);

typedef struct aegis_socket {
    bool used;
    u32 owner_pid;
    u32 type;
    u16 local_port;
    u16 remote_port;
    u32 remote_ip;
    int transport_id;
    bool listening;
    bool connected;
} aegis_socket_t;

static aegis_socket_t sockets[AEGIS_SOCKET_MAX];
static u32 listening_count, connected_count;
static void zero(void*p,u64 n){u8*b=p;for(u64 i=0;i<n;i++)b[i]=0;}
static aegis_socket_t *lookup(u32 owner,int fd){int i=fd-(int)AEGIS_SOCKET_FD_BASE;if(i<0||i>=(int)AEGIS_SOCKET_MAX)return NULL;if(!sockets[i].used||sockets[i].owner_pid!=owner)return NULL;return &sockets[i];}

void net_socket_init(void){zero(sockets,sizeof(sockets));listening_count=0;connected_count=0;}
int net_socket_create(u32 owner,u32 type){if(!owner||(type!=AEGIS_SOCK_STREAM&&type!=AEGIS_SOCK_DGRAM))return AEGIS_EINVAL;for(u32 i=0;i<AEGIS_SOCKET_MAX;i++)if(!sockets[i].used){zero(&sockets[i],sizeof(sockets[i]));sockets[i].used=true;sockets[i].owner_pid=owner;sockets[i].type=type;sockets[i].transport_id=-1;return (int)(AEGIS_SOCKET_FD_BASE+i);}return AEGIS_ENOMEM;}
int net_socket_close(u32 owner,int fd){aegis_socket_t*s=lookup(owner,fd);if(!s)return AEGIS_ENOENT;if(s->type==AEGIS_SOCK_DGRAM&&s->transport_id>=0)udp_unbind(s->transport_id);if(s->type==AEGIS_SOCK_STREAM&&s->transport_id>=0)tcp_close_id(s->transport_id);if(s->listening&&listening_count)listening_count--;if(s->connected&&connected_count)connected_count--;zero(s,sizeof(*s));return AEGIS_OK;}
void net_socket_close_all(u32 owner){for(u32 i=0;i<AEGIS_SOCKET_MAX;i++){if(sockets[i].used&&sockets[i].owner_pid==owner)(void)net_socket_close(owner,(int)(AEGIS_SOCKET_FD_BASE+i));}}
int net_socket_bind(u32 owner,int fd,u16 port){aegis_socket_t*s=lookup(owner,fd);if(!s||!port)return AEGIS_EINVAL;if(s->local_port)return AEGIS_EBUSY;s->local_port=port;if(s->type==AEGIS_SOCK_DGRAM){s->transport_id=udp_bind(port);if(s->transport_id<0)return AEGIS_EBUSY;}return AEGIS_OK;}
int net_socket_listen(u32 owner,int fd){aegis_socket_t*s=lookup(owner,fd);if(!s||s->type!=AEGIS_SOCK_STREAM||!s->local_port)return AEGIS_EINVAL;s->transport_id=tcp_listen(s->local_port);if(s->transport_id<0)return AEGIS_ENOMEM;s->listening=true;listening_count++;return AEGIS_OK;}
int net_socket_accept(u32 owner,int fd){aegis_socket_t*l=lookup(owner,fd);if(!l||!l->listening)return AEGIS_EINVAL;network_poll();int tid=tcp_accept(l->transport_id);if(tid<0)return AEGIS_EAGAIN;int child=net_socket_create(owner,AEGIS_SOCK_STREAM);if(child<0)return child;aegis_socket_t*c=lookup(owner,child);c->local_port=l->local_port;c->transport_id=tid;c->connected=true;connected_count++;return child;}
int net_socket_connect(u32 owner,int fd,u32 ip,u16 port){aegis_socket_t*s=lookup(owner,fd);if(!s||s->type!=AEGIS_SOCK_STREAM||!ip||!port)return AEGIS_EINVAL;s->transport_id=tcp_connect(ip,port);if(s->transport_id<0)return AEGIS_EIO;s->remote_ip=ip;s->remote_port=port;return AEGIS_OK;}
int net_socket_send(u32 owner,int fd,const void*data,u32 len){aegis_socket_t*s=lookup(owner,fd);if(!s||!data||!len||len>65535U)return AEGIS_EINVAL;if(s->type==AEGIS_SOCK_DGRAM){if(!s->local_port||!s->remote_ip||!s->remote_port)return AEGIS_EINVAL;return udp_send(s->remote_ip,s->remote_port,s->local_port,data,(u16)len)==0?(int)len:AEGIS_EIO;}if(!s->connected&&tcp_is_established(s->transport_id)){s->connected=true;connected_count++;}return tcp_send_id(s->transport_id,data,(u16)len);}
int net_socket_recv(u32 owner,int fd,void*data,u32 len){aegis_socket_t*s=lookup(owner,fd);if(!s||!data||!len)return AEGIS_EINVAL;network_poll();if(s->type==AEGIS_SOCK_DGRAM){int rc=udp_recv(s->transport_id,data,(u16)(len>65535U?65535U:len),&s->remote_ip,&s->remote_port);return rc<0?AEGIS_EAGAIN:rc;}int rc=tcp_recv_id(s->transport_id,data,(u16)(len>65535U?65535U:len));return rc<0?AEGIS_EAGAIN:rc;}
void network_poll(void){for(u32 n=0;n<32U;n++){pktbuf_t*p=pktbuf_alloc();if(!p)return;u16 len=0;int rc=ethernet_recv(p->data+p->head,&len);if(rc!=AEGIS_OK){pktbuf_free(p);return;}p->len=len;ethernet_rx(p);}}
u32 net_socket_listening_count(void){return listening_count;}u32 net_socket_connected_count(void){return connected_count;}

int net_socket_selftest(void) {
    const u32 owner = 0xfffffff0U;
    const u32 before = listening_count;

    int stream = net_socket_create(owner, AEGIS_SOCK_STREAM);
    if (stream < 0) return stream;
    int rc = net_socket_bind(owner, stream, 49152U);
    if (rc != AEGIS_OK) { (void)net_socket_close(owner, stream); return rc; }
    rc = net_socket_listen(owner, stream);
    if (rc != AEGIS_OK || listening_count != before + 1U) {
        (void)net_socket_close(owner, stream);
        return AEGIS_EIO;
    }
    rc = net_socket_close(owner, stream);
    if (rc != AEGIS_OK || listening_count != before) return AEGIS_EIO;

    int datagram = net_socket_create(owner, AEGIS_SOCK_DGRAM);
    if (datagram < 0) return datagram;
    rc = net_socket_bind(owner, datagram, 49153U);
    if (rc != AEGIS_OK) { (void)net_socket_close(owner, datagram); return rc; }
    rc = net_socket_close(owner, datagram);
    return rc == AEGIS_OK ? AEGIS_OK : AEGIS_EIO;
}
