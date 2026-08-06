/* SPDX-License-Identifier: Proprietary */
#include "installer.h"
#include "block_storage.h"

#define COPY_SECTORS 8U
static u8 copy_buf[COPY_SECTORS*512U]__attribute__((aligned(4096)));
static u8 verify_buf[COPY_SECTORS*512U]__attribute__((aligned(4096)));
static aegis_install_result_t last;
static void zero(void*p,u64 n){u8*b=p;for(u64 i=0;i<n;i++)b[i]=0;}
static u32 crc32_update(u32 crc,const u8*p,u64 n){crc=~crc;for(u64 i=0;i<n;i++){crc^=p[i];for(u32 b=0;b<8;b++)crc=(crc>>1)^(0xedb88320U&-(s32)(crc&1U));}return~crc;}
int aegis_installer_install_to_nvme(aegis_install_result_t*out){zero(&last,sizeof(last));const aegis_block_registry_t*st=block_storage_state();if(!st||!st->io_backend_ready)return AEGIS_ENOENT;const aegis_block_device_t*src=NULL,*dst=NULL;for(u32 i=0;i<st->device_count;i++){const aegis_block_device_t*d=block_storage_device(i);if(!d||d->state!=AEGIS_BLOCK_DEVICE_READY)continue;if((d->flags&AEGIS_BLOCK_FLAG_NVME)!=0U)dst=d;else if(!src&&(d->flags&AEGIS_BLOCK_FLAG_READABLE)!=0U)src=d;}if(!src||!dst||src->id==dst->id)return AEGIS_ENOENT;u64 total=src->block_count<dst->block_count?src->block_count:dst->block_count;u32 crc_s=0,crc_d=0;for(u64 lba=0;lba<total;){u32 count=(u32)((total-lba)>COPY_SECTORS?COPY_SECTORS:(total-lba));int rc=block_storage_read(src->id,lba,count,copy_buf);if(rc!=AEGIS_OK){last.status=rc;return rc;}rc=block_storage_write(dst->id,lba,count,copy_buf);if(rc!=AEGIS_OK){last.status=rc;return rc;}rc=block_storage_read(dst->id,lba,count,verify_buf);if(rc!=AEGIS_OK){last.status=rc;return rc;}u64 bytes=(u64)count*512U;crc_s=crc32_update(crc_s,copy_buf,bytes);crc_d=crc32_update(crc_d,verify_buf,bytes);for(u64 i=0;i<bytes;i++)if(copy_buf[i]!=verify_buf[i]){last.status=AEGIS_EIO;return AEGIS_EIO;}lba+=count;last.sectors_copied=lba;last.sectors_verified=lba;}int rc=block_storage_flush(dst->id);last.source_device=src->id;last.target_device=dst->id;last.crc_source=crc_s;last.crc_target=crc_d;last.status=(rc==AEGIS_OK&&crc_s==crc_d)?AEGIS_OK:AEGIS_EIO;if(out)*out=last;return last.status;}
const aegis_install_result_t*aegis_installer_last_result(void){return&last;}
