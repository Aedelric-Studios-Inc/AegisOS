/* SPDX-License-Identifier: Proprietary */
#include "update.h"
#include "block_storage.h"

#define UPDATE_MAGIC 0x55474541U /* AEGU */
#define UPDATE_FORMAT 1U
#define UPDATE_METADATA_LBA 96U

typedef struct update_disk {
    u32 magic,format,crc32,reserved;
    u64 generation;
    u8 active_slot,pending_slot,pending,rollback_required;
    u32 boot_attempts,max_boot_attempts;
    char active_version[AEGIS_UPDATE_VERSION_MAX];
    char pending_version[AEGIS_UPDATE_VERSION_MAX];
    u8 active_hash[32],pending_hash[32];
    u8 pad[512U-4U*4U-8U-4U-8U-64U-64U];
}__attribute__((packed, aligned(8)))update_disk_t;
_Static_assert(sizeof(update_disk_t)==512U,"update metadata sector");
_Static_assert(_Alignof(update_disk_t)==8U,"update metadata alignment");
_Static_assert(__builtin_offsetof(update_disk_t,generation)%8U==0U,"update generation alignment");
_Static_assert(__builtin_offsetof(update_disk_t,boot_attempts)%4U==0U,"update boot-attempt alignment");
static aegis_update_state_t g;
static const aegis_block_partition_t*part;
static void zero(void*p,u64 n){u8*b=p;for(u64 i=0;i<n;i++)b[i]=0;}
static void copy(void*d,const void*s,u64 n){u8*x=d;const u8*y=s;for(u64 i=0;i<n;i++)x[i]=y[i];}
static void strcopy(char*d,const char*s,u32 n){u32 i=0;if(!s)s="";for(;i+1<n&&s[i];i++)d[i]=s[i];d[i]=0;}
static u32 crc32(const void*data,u64 n){const u8*p=data;u32 c=~0U;for(u64 i=0;i<n;i++){c^=p[i];for(u32 b=0;b<8;b++)c=(c>>1)^(0xedb88320U&-(s32)(c&1U));}return~c;}
static u32 disk_crc(update_disk_t*d){u32 saved=d->crc32;d->crc32=0;u32 c=crc32(d,sizeof(*d));d->crc32=saved;return c;}
static int store(void){if(!part)return AEGIS_ENOENT;update_disk_t d;zero(&d,sizeof(d));d.magic=UPDATE_MAGIC;d.format=UPDATE_FORMAT;d.generation=g.generation;d.active_slot=g.active_slot;d.pending_slot=g.pending_slot;d.pending=g.pending;d.rollback_required=g.rollback_required;d.boot_attempts=g.boot_attempts;d.max_boot_attempts=g.max_boot_attempts;strcopy(d.active_version,g.active_version,sizeof(d.active_version));strcopy(d.pending_version,g.pending_version,sizeof(d.pending_version));copy(d.active_hash,g.active_hash,32);copy(d.pending_hash,g.pending_hash,32);d.crc32=disk_crc(&d);int rc=block_partition_write(part,UPDATE_METADATA_LBA,1,&d);if(rc==AEGIS_OK)rc=block_storage_flush(part->device_id);return rc;}
static void defaults(void){zero(&g,sizeof(g));g.initialised=true;g.metadata_valid=true;g.active_slot=AEGIS_UPDATE_SLOT_A;g.pending_slot=AEGIS_UPDATE_SLOT_NONE;g.max_boot_attempts=3;strcopy(g.active_version,"2.0.0-development",sizeof(g.active_version));}
int update_init(void){defaults();part=block_storage_find_partition("AEGIS_CONFIG");if(!part||!block_storage_io_backend_ready())return AEGIS_ENOENT;update_disk_t d;int rc=block_partition_read(part,UPDATE_METADATA_LBA,1,&d);if(rc!=AEGIS_OK||d.magic!=UPDATE_MAGIC||d.format!=UPDATE_FORMAT||disk_crc(&d)!=d.crc32){g.generation=1;return store();}g.generation=d.generation;g.active_slot=(aegis_update_slot_t)d.active_slot;g.pending_slot=(aegis_update_slot_t)d.pending_slot;g.pending=d.pending;g.rollback_required=d.rollback_required;g.boot_attempts=d.boot_attempts;g.max_boot_attempts=d.max_boot_attempts?d.max_boot_attempts:3;strcopy(g.active_version,d.active_version,sizeof(g.active_version));strcopy(g.pending_version,d.pending_version,sizeof(g.pending_version));copy(g.active_hash,d.active_hash,32);copy(g.pending_hash,d.pending_hash,32);if(g.pending&&g.boot_attempts>=g.max_boot_attempts){g.rollback_required=true;g.pending=false;g.pending_slot=AEGIS_UPDATE_SLOT_NONE;g.generation++;(void)store();}return AEGIS_OK;}
int update_stage(aegis_update_slot_t slot,const char*version,const u8 hash[32]){if(!g.initialised||!part||!version||!hash||slot>AEGIS_UPDATE_SLOT_B||slot==g.active_slot)return AEGIS_EINVAL;g.pending=true;g.rollback_required=false;g.pending_slot=slot;g.boot_attempts=0;strcopy(g.pending_version,version,sizeof(g.pending_version));copy(g.pending_hash,hash,32);g.generation++;return store();}
int update_note_boot_attempt(void){if(!g.pending)return AEGIS_OK;g.boot_attempts++;if(g.boot_attempts>=g.max_boot_attempts)g.rollback_required=true;g.generation++;return store();}
int update_mark_boot_success(void){if(!g.pending)return AEGIS_EINVAL;g.active_slot=g.pending_slot;g.pending_slot=AEGIS_UPDATE_SLOT_NONE;g.pending=false;g.rollback_required=false;g.boot_attempts=0;strcopy(g.active_version,g.pending_version,sizeof(g.active_version));copy(g.active_hash,g.pending_hash,32);zero(g.pending_version,sizeof(g.pending_version));zero(g.pending_hash,sizeof(g.pending_hash));g.generation++;return store();}
int update_request_rollback(void){g.rollback_required=true;g.pending=false;g.pending_slot=AEGIS_UPDATE_SLOT_NONE;g.boot_attempts=0;g.generation++;return store();}
const aegis_update_state_t*update_state(void){return&g;}
