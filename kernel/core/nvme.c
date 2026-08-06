/* SPDX-License-Identifier: Proprietary */
#include "nvme.h"
#include "pci.h"
#include "block_storage.h"
#include "device_tree.h"

#define NVME_QD 32U
#define NVME_WAIT 100000000ULL
#define NVME_ADMIN_IDENTIFY 0x06U
#define NVME_ADMIN_CREATE_CQ 0x05U
#define NVME_ADMIN_CREATE_SQ 0x01U
#define NVME_IO_WRITE 0x01U
#define NVME_IO_READ 0x02U
#define NVME_IO_FLUSH 0x00U

typedef struct nvme_cmd{u8 opc,flags;u16 cid;u32 nsid;u64 rsvd2;u64 mptr;u64 prp1,prp2;u32 cdw10,cdw11,cdw12,cdw13,cdw14,cdw15;}__attribute__((packed))nvme_cmd_t;
typedef struct nvme_cpl{u32 result;u32 rsvd;u16 sq_head,sq_id;u16 cid,status;}__attribute__((packed))nvme_cpl_t;
typedef struct nvme_q{nvme_cmd_t*sq;nvme_cpl_t*cq;u16 qid,depth,sq_tail,cq_head;u8 phase;volatile u32*sq_db;volatile u32*cq_db;}nvme_q_t;
static nvme_cmd_t admin_sq[NVME_QD]__attribute__((aligned(4096))),io_sq[NVME_QD]__attribute__((aligned(4096)));
static nvme_cpl_t admin_cq[NVME_QD]__attribute__((aligned(4096))),io_cq[NVME_QD]__attribute__((aligned(4096)));
static u8 identify_buf[4096]__attribute__((aligned(4096))),io_bounce[4096]__attribute__((aligned(4096)));
static volatile u8*g_regs;static nvme_q_t aq,ioq;static u16 next_cid=1;static bool ready;static u64 sectors;static u32 block_id;
static u32 r32(u32 o){return*(volatile u32*)(g_regs+o);}static u64 r64(u32 o){return*(volatile u64*)(g_regs+o);}static void w32(u32 o,u32 v){*(volatile u32*)(g_regs+o)=v;}static void w64(u32 o,u64 v){*(volatile u64*)(g_regs+o)=v;}static void zero(void*p,u64 n){u8*b=p;for(u64 i=0;i<n;i++)b[i]=0;}static void copy(void*d,const void*s,u64 n){u8*x=d;const u8*y=s;for(u64 i=0;i<n;i++)x[i]=y[i];}
static int wait_ready(bool want){for(u64 i=0;i<NVME_WAIT;i++)if(((r32(0x1c)&1U)!=0)==want)return AEGIS_OK;return AEGIS_ETIMEDOUT;}
static void q_setup(nvme_q_t*q,u16 id,nvme_cmd_t*sq,nvme_cpl_t*cq,u32 stride){zero(sq,sizeof(nvme_cmd_t)*NVME_QD);zero(cq,sizeof(nvme_cpl_t)*NVME_QD);q->sq=sq;q->cq=cq;q->qid=id;q->depth=NVME_QD;q->sq_tail=0;q->cq_head=0;q->phase=1;q->sq_db=(volatile u32*)(g_regs+0x1000+(2U*id)*stride);q->cq_db=(volatile u32*)(g_regs+0x1000+(2U*id+1U)*stride);}
static int submit(nvme_q_t*q,nvme_cmd_t*c){u16 cid=next_cid++;if(!next_cid)next_cid=1;c->cid=cid;copy(&q->sq[q->sq_tail],c,sizeof(*c));__asm__ volatile("dmb sy":::"memory");q->sq_tail=(q->sq_tail+1U)%q->depth;*q->sq_db=q->sq_tail;for(u64 spin=0;spin<NVME_WAIT;spin++){nvme_cpl_t*e=&q->cq[q->cq_head];if((e->status&1U)==q->phase){__asm__ volatile("dmb sy":::"memory");u16 st=e->status>>1;if(e->cid!=cid)return AEGIS_EIO;q->cq_head=(q->cq_head+1U)%q->depth;if(q->cq_head==0)q->phase^=1U;*q->cq_db=q->cq_head;return st?AEGIS_EIO:AEGIS_OK;}}return AEGIS_ETIMEDOUT;}
static int identify(u32 nsid,u32 cns){zero(identify_buf,sizeof(identify_buf));nvme_cmd_t c;zero(&c,sizeof(c));c.opc=NVME_ADMIN_IDENTIFY;c.nsid=nsid;c.prp1=(u64)(uptr)identify_buf;c.cdw10=cns;return submit(&aq,&c);}
static int io_one(bool write,u64 lba,void*buf){nvme_cmd_t c;zero(&c,sizeof(c));if(write)copy(io_bounce,buf,512);c.opc=write?NVME_IO_WRITE:NVME_IO_READ;c.nsid=1;c.prp1=(u64)(uptr)io_bounce;c.cdw10=(u32)lba;c.cdw11=(u32)(lba>>32);c.cdw12=0;int rc=submit(&ioq,&c);if(rc==AEGIS_OK&&!write)copy(buf,io_bounce,512);return rc;}
static int blk_read(void*ctx,u64 lba,u32 count,void*buf){(void)ctx;u8*p=buf;for(u32 i=0;i<count;i++){int rc=io_one(false,lba+i,p+i*512U);if(rc!=AEGIS_OK)return rc;}return AEGIS_OK;}
static int blk_write(void*ctx,u64 lba,u32 count,const void*buf){(void)ctx;const u8*p=buf;for(u32 i=0;i<count;i++){int rc=io_one(true,lba+i,(void*)(p+i*512U));if(rc!=AEGIS_OK)return rc;}return AEGIS_OK;}
static int blk_flush(void*ctx){(void)ctx;nvme_cmd_t c;zero(&c,sizeof(c));c.opc=NVME_IO_FLUSH;c.nsid=1;return submit(&ioq,&c);}static const aegis_block_ops_t ops={blk_read,blk_write,blk_flush};
int nvme_init(void){ready=false;sectors=0;const aegis_dtb_platform_t*plat=device_tree_platform();if(!plat||!plat->valid||!plat->pci_ecam_base)return AEGIS_ENOENT;pci_init(plat->pci_ecam_base,plat->pci_bus_start,plat->pci_bus_end);if(pci_scan()!=AEGIS_OK)return AEGIS_ENOENT;const aegis_pci_device_t*d=pci_find_class(1,8,2,0);if(!d||!d->bar[0])return AEGIS_ENOENT;(void)pci_enable_memory_busmaster(d);g_regs=(volatile u8*)(uptr)d->bar[0];u64 cap=r64(0);u32 stride=4U<<(u32)((cap>>32)&0xfU);w32(0x14,0);if(wait_ready(false)!=AEGIS_OK)return AEGIS_ETIMEDOUT;q_setup(&aq,0,admin_sq,admin_cq,stride);w32(0x24,((NVME_QD-1U)<<16)|(NVME_QD-1U));w64(0x28,(u64)(uptr)admin_sq);w64(0x30,(u64)(uptr)admin_cq);w32(0x14,(6U<<16)|(4U<<20)|1U);if(wait_ready(true)!=AEGIS_OK)return AEGIS_ETIMEDOUT;if(identify(0,1)!=AEGIS_OK)return AEGIS_EIO;
q_setup(&ioq,1,io_sq,io_cq,stride);nvme_cmd_t c;zero(&c,sizeof(c));c.opc=NVME_ADMIN_CREATE_CQ;c.prp1=(u64)(uptr)io_cq;c.cdw10=((NVME_QD-1U)<<16)|1U;c.cdw11=1U;if(submit(&aq,&c)!=AEGIS_OK)return AEGIS_EIO;zero(&c,sizeof(c));c.opc=NVME_ADMIN_CREATE_SQ;c.prp1=(u64)(uptr)io_sq;c.cdw10=((NVME_QD-1U)<<16)|1U;c.cdw11=(1U<<16)|1U;if(submit(&aq,&c)!=AEGIS_OK)return AEGIS_EIO;if(identify(1,0)!=AEGIS_OK)return AEGIS_EIO;sectors=*(u64*)&identify_buf[0];if(!sectors)return AEGIS_EIO;int rc=block_storage_register_device("nvme0n1",512,sectors,AEGIS_BLOCK_FLAG_READABLE|AEGIS_BLOCK_FLAG_WRITABLE|AEGIS_BLOCK_FLAG_PERSISTENT|AEGIS_BLOCK_FLAG_NVME,&ops,NULL,&block_id);if(rc!=AEGIS_OK)return rc;ready=true;return AEGIS_OK;}
bool nvme_ready(void){return ready;}u64 nvme_capacity_sectors(void){return sectors;}
