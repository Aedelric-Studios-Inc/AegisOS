/* SPDX-License-Identifier: Proprietary */
/* AegisOS virtio-net MMIO driver for QEMU virt and AegisBox development. */
#include "../../include/ethernet.h"
#include "../../../kernel/include/virtio_mmio.h"

#define VIRTIO_NET_F_MAC       (1ULL << 5)
#define VIRTIO_NET_QUEUE_SIZE  64U
#define VIRTIO_NET_HDR_SIZE    12U
#define VIRTIO_NET_FRAME_MAX   (ETH_MTU + 14U)
#define VIRTIO_NET_BUF_SIZE    (VIRTIO_NET_HDR_SIZE + VIRTIO_NET_FRAME_MAX)

typedef struct virtio_net_hdr {
    u8 flags;
    u8 gso_type;
    u16 hdr_len;
    u16 gso_size;
    u16 csum_start;
    u16 csum_offset;
    u16 num_buffers;
} __attribute__((packed)) virtio_net_hdr_t;

typedef struct net_queue_mem {
    virtq_desc_t desc[VIRTIO_NET_QUEUE_SIZE];
    virtq_avail_t avail;
    virtq_used_t used;
} __attribute__((aligned(4096))) net_queue_mem_t;

static virtio_mmio_device_t *g_dev;
static virtio_queue_t g_rxq, g_txq;
static net_queue_mem_t g_rxmem, g_txmem;
static u8 g_rxbuf[VIRTIO_NET_QUEUE_SIZE][VIRTIO_NET_BUF_SIZE] __attribute__((aligned(16)));
static u8 g_txbuf[VIRTIO_NET_BUF_SIZE] __attribute__((aligned(16)));
static u8 g_mac[ETH_ALEN] = {0x02,0xae,0x61,0x5b,0x0c,0x01};
static bool g_ready;

static volatile u8 *reg8(u64 base,u32 off){return (volatile u8 *)(uptr)(base+off);}
static void copy_bytes(void*d,const void*s,u32 n){u8*dd=d;const u8*ss=s;for(u32 i=0;i<n;i++)dd[i]=ss[i];}

int ethernet_init(void) {
    g_ready=false;
    g_dev=virtio_mmio_device_by_type(VIRTIO_DEVICE_NET,0U);
    if(!g_dev)return AEGIS_ENOENT;
    int rc=virtio_mmio_begin(g_dev,VIRTIO_F_VERSION_1|VIRTIO_NET_F_MAC);if(rc!=AEGIS_OK)return rc;
    rc=virtio_mmio_setup_queue(g_dev,&g_rxq,0U,VIRTIO_NET_QUEUE_SIZE,g_rxmem.desc,&g_rxmem.avail,&g_rxmem.used);if(rc!=AEGIS_OK)return rc;
    rc=virtio_mmio_setup_queue(g_dev,&g_txq,1U,VIRTIO_NET_QUEUE_SIZE,g_txmem.desc,&g_txmem.avail,&g_txmem.used);if(rc!=AEGIS_OK)return rc;
    for(u32 i=0;i<ETH_ALEN;i++)g_mac[i]=*reg8(g_dev->base,0x100U+i);
    bool all_zero=true;for(u32 i=0;i<ETH_ALEN;i++)if(g_mac[i])all_zero=false;
    if(all_zero){u8 fallback[ETH_ALEN]={0x02,0xae,0x61,0x5b,0x0c,0x01};copy_bytes(g_mac,fallback,ETH_ALEN);}
    for(u16 i=0;i<g_rxq.size;i++){
        g_rxq.desc[i].addr=(u64)(uptr)g_rxbuf[i];g_rxq.desc[i].len=VIRTIO_NET_BUF_SIZE;g_rxq.desc[i].flags=VIRTQ_DESC_F_WRITE;g_rxq.desc[i].next=0U;
        g_rxq.avail->ring[i]=i;
    }
    g_rxq.avail->idx=g_rxq.size;
    virtio_memory_barrier();
    virtio_mmio_finish(g_dev);
    virtio_mmio_notify(&g_rxq);
    g_ready=true;
    return AEGIS_OK;
}

int ethernet_send(const u8 *frame,u16 len){
    if(!g_ready||!frame||len==0U||len>VIRTIO_NET_FRAME_MAX)return AEGIS_EINVAL;
    for(u32 i=0;i<VIRTIO_NET_HDR_SIZE;i++)g_txbuf[i]=0U;copy_bytes(g_txbuf+VIRTIO_NET_HDR_SIZE,frame,len);
    g_txq.desc[0].addr=(u64)(uptr)g_txbuf;g_txq.desc[0].len=VIRTIO_NET_HDR_SIZE+len;g_txq.desc[0].flags=0U;g_txq.desc[0].next=0U;
    u16 idx=g_txq.avail->idx;g_txq.avail->ring[idx%g_txq.size]=0U;virtio_memory_barrier();g_txq.avail->idx=(u16)(idx+1U);virtio_mmio_notify(&g_txq);
    u32 spins=0U;while(virtq_used_load_idx(g_txq.used)==g_txq.last_used_idx){__asm__ volatile("yield");if(++spins>10000000U)return AEGIS_ETIMEDOUT;}
    g_txq.last_used_idx=virtq_used_load_idx(g_txq.used);return AEGIS_OK;
}

int ethernet_recv(u8 *buf,u16 *len){
    if(!g_ready||!buf||!len)return AEGIS_EINVAL;
    if(virtq_used_load_idx(g_rxq.used)==g_rxq.last_used_idx)return AEGIS_EAGAIN;
    virtio_memory_barrier();
    virtq_used_elem_t *elem=&g_rxq.used->ring[g_rxq.last_used_idx%g_rxq.size];
    u32 id=virtq_used_elem_load_id(elem);u32 total=virtq_used_elem_load_len(elem);if(id>=g_rxq.size||total<VIRTIO_NET_HDR_SIZE){g_rxq.last_used_idx++;return AEGIS_EIO;}
    u32 frame_len=total-VIRTIO_NET_HDR_SIZE;if(frame_len>VIRTIO_NET_FRAME_MAX)frame_len=VIRTIO_NET_FRAME_MAX;
    copy_bytes(buf,g_rxbuf[id]+VIRTIO_NET_HDR_SIZE,frame_len);*len=(u16)frame_len;g_rxq.last_used_idx++;
    u16 aidx=g_rxq.avail->idx;g_rxq.avail->ring[aidx%g_rxq.size]=(u16)id;virtio_memory_barrier();g_rxq.avail->idx=(u16)(aidx+1U);virtio_mmio_notify(&g_rxq);
    return AEGIS_OK;
}
void ethernet_get_mac(eth_addr_t*out){if(!out)return;for(u32 i=0;i<ETH_ALEN;i++)out->mac[i]=g_mac[i];}
bool ethernet_link_ready(void){return g_ready;}
