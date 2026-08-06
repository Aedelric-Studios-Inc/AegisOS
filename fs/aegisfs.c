/* SPDX-License-Identifier: Proprietary */
/* AegisFS: small crash-detecting persistent filesystem for AEGIS_CONFIG. */
#include "include/aegisfs.h"
#include "include/vfs.h"
#include "../kernel/include/block_storage.h"

#define AEGISFS_MAGIC 0x53464741U /* AGFS */
#define AEGISFS_VERSION 1U
#define AEGISFS_MAX_FILES 64U
#define AEGISFS_NAME_MAX 80U
#define AEGISFS_DIR_LBA 1U
#define AEGISFS_DIR_SECTORS 16U
#define AEGISFS_DATA_LBA (AEGISFS_DIR_LBA + AEGISFS_DIR_SECTORS)
#define AEGISFS_MAX_FILE_BYTES (64U * 1024U)

typedef struct aegisfs_superblock {
    u32 magic;
    u32 version;
    u32 generation;
    u32 file_count;
    u64 next_free_lba;
    u64 partition_blocks;
    u32 directory_crc;
    u32 flags;
    u8 reserved[472];
} __attribute__((packed)) aegisfs_superblock_t;

typedef struct aegisfs_entry {
    u32 valid;
    u32 generation;
    u64 start_lba;
    u32 sectors;
    u32 size;
    u32 data_crc;
    char name[AEGISFS_NAME_MAX];
    u8 reserved[20];
} __attribute__((packed)) aegisfs_entry_t;

static const aegis_block_partition_t *g_part;
static aegisfs_superblock_t g_sb;
static aegisfs_entry_t g_entries[AEGISFS_MAX_FILES];
static bool g_mounted;
static bool g_writable;

static void zero_bytes(void *ptr, u64 len) { u8 *p=(u8 *)ptr; for(u64 i=0;i<len;i++)p[i]=0; }
static void copy_bytes(void *dst,const void *src,u64 len){u8*d=dst;const u8*s=src;for(u64 i=0;i<len;i++)d[i]=s[i];}
static bool streq(const char*a,const char*b){if(!a||!b)return false;while(*a&&*b){if(*a++!=*b++)return false;}return *a=='\0'&&*b=='\0';}
static void copy_name(char *dst,const char*src){u32 i=0;while(src&&src[i]&&i+1U<AEGISFS_NAME_MAX){dst[i]=src[i];i++;}dst[i]='\0';}
static u32 crc32(const void *data,u64 len){const u8*p=data;u32 c=0xffffffffU;for(u64 i=0;i<len;i++){c^=p[i];for(u32 j=0;j<8;j++)c=(c>>1)^((0U-(c&1U))&0xedb88320U);}return ~c;}

static int persist_metadata(void) {
    if (!g_part || !g_writable) return AEGIS_ESHUTDOWN;
    g_sb.directory_crc = crc32(g_entries, sizeof(g_entries));
    int rc = block_partition_write(g_part, AEGISFS_DIR_LBA, AEGISFS_DIR_SECTORS, g_entries);
    if (rc != AEGIS_OK) return rc;
    rc = block_partition_write(g_part, 0U, 1U, &g_sb);
    if (rc != AEGIS_OK) return rc;
    return block_storage_flush(g_part->device_id);
}

static int format_fs(void) {
    zero_bytes(&g_sb, sizeof(g_sb));
    zero_bytes(g_entries, sizeof(g_entries));
    g_sb.magic = AEGISFS_MAGIC;
    g_sb.version = AEGISFS_VERSION;
    g_sb.generation = 1U;
    g_sb.next_free_lba = AEGISFS_DATA_LBA;
    g_sb.partition_blocks = g_part->block_count;
    return persist_metadata();
}

static aegisfs_entry_t *find_entry(const char *name) {
    while (name && *name == '/') name++;
    for (u32 i=0;i<AEGISFS_MAX_FILES;i++) if(g_entries[i].valid&&streq(g_entries[i].name,name)) return &g_entries[i];
    return NULL;
}
static aegisfs_entry_t *alloc_entry(const char *name) {
    for(u32 i=0;i<AEGISFS_MAX_FILES;i++) if(!g_entries[i].valid){zero_bytes(&g_entries[i],sizeof(g_entries[i]));g_entries[i].valid=1U;copy_name(g_entries[i].name,name);g_sb.file_count++;return &g_entries[i];}
    return NULL;
}

static int agfs_read(vnode_t *vn,u64 offset,void *buf,u64 len){
    if(!vn||!buf)return AEGIS_EINVAL; aegisfs_entry_t*e=find_entry((const char*)vn->priv); if(!e)return AEGIS_ENOENT;
    if(offset>=e->size)return 0; if(len>e->size-offset)len=e->size-offset;
    u64 first=offset/512U,last=(offset+len+511U)/512U;u32 sectors=(u32)(last-first); if(sectors>128U)return AEGIS_EINVAL;
    static u8 bounce[AEGISFS_MAX_FILE_BYTES] __attribute__((aligned(16)));
    int rc=block_partition_read(g_part,e->start_lba+first,sectors,bounce);if(rc!=AEGIS_OK)return rc;
    copy_bytes(buf,bounce+(offset%512U),len);return (int)len;
}
static int agfs_write(vnode_t *vn,u64 offset,const void *buf,u64 len){
    if(!vn||!buf||!g_writable||len==0U)return AEGIS_EINVAL;if(offset+len>AEGISFS_MAX_FILE_BYTES)return AEGIS_ENOSPC;
    const char*name=(const char*)vn->priv;while(*name=='/')name++;if(!*name)return AEGIS_EINVAL;
    aegisfs_entry_t*e=find_entry(name);if(!e)e=alloc_entry(name);if(!e)return AEGIS_ENOSPC;
    u32 new_size=(u32)(offset+len);if(new_size<e->size)new_size=e->size;u32 sectors=(new_size+511U)/512U;
    if(g_sb.next_free_lba+sectors>g_part->block_count)return AEGIS_ENOSPC;
    static u8 image[AEGISFS_MAX_FILE_BYTES] __attribute__((aligned(16)));zero_bytes(image,(u64)sectors*512U);
    if(e->size){u32 old_sectors=(e->size+511U)/512U;int rc=block_partition_read(g_part,e->start_lba,old_sectors,image);if(rc!=AEGIS_OK)return rc;}
    copy_bytes(image+offset,buf,len);
    u64 new_lba=g_sb.next_free_lba;int rc=block_partition_write(g_part,new_lba,sectors,image);if(rc!=AEGIS_OK)return rc;
    e->start_lba=new_lba;e->sectors=sectors;e->size=new_size;e->generation=++g_sb.generation;e->data_crc=crc32(image,new_size);g_sb.next_free_lba+=sectors;
    rc=persist_metadata();if(rc==AEGIS_OK)vn->size=e->size;return rc==AEGIS_OK?(int)len:rc;
}
static int agfs_readdir(vnode_t*vn,u32 index,char*name,vnode_t**out){(void)vn;(void)out;if(!name)return AEGIS_EINVAL;u32 seen=0;for(u32 i=0;i<AEGISFS_MAX_FILES;i++){if(!g_entries[i].valid)continue;if(seen++==index){copy_name(name,g_entries[i].name);return AEGIS_OK;}}return AEGIS_ENOENT;}
static int agfs_stat(vnode_t*vn,void*out){if(!vn||!out)return AEGIS_EINVAL;aegisfs_entry_t*e=find_entry((const char*)vn->priv);if(!e)return AEGIS_ENOENT;*(u64*)out=e->size;return AEGIS_OK;}
static vnode_ops_t g_ops={agfs_read,agfs_write,agfs_readdir,agfs_stat};
static vfs_t g_fs={"aegisfs",NULL,&g_ops,NULL};

int aegisfs_mount_persistent(void){
    if(g_mounted)return AEGIS_OK;g_part=block_storage_find_partition("AEGIS_CONFIG");if(!g_part)return AEGIS_ENOENT;
    g_writable=block_storage_io_backend_ready()&&(g_part->flags&AEGIS_BLOCK_FLAG_WRITABLE);
    if(!g_writable)return AEGIS_ENOSYS;
    int rc=block_partition_read(g_part,0U,1U,&g_sb);if(rc!=AEGIS_OK)return rc;
    if(g_sb.magic!=AEGISFS_MAGIC||g_sb.version!=AEGISFS_VERSION||g_sb.partition_blocks!=g_part->block_count){rc=format_fs();if(rc!=AEGIS_OK)return rc;}
    else {rc=block_partition_read(g_part,AEGISFS_DIR_LBA,AEGISFS_DIR_SECTORS,g_entries);if(rc!=AEGIS_OK)return rc;if(crc32(g_entries,sizeof(g_entries))!=g_sb.directory_crc)return AEGIS_EIO;}
    rc=vfs_mount("/persist",&g_fs);if(rc!=AEGIS_OK)return rc;g_mounted=true;return AEGIS_OK;
}
bool aegisfs_is_mounted(void){return g_mounted;}bool aegisfs_is_writable(void){return g_writable;}u32 aegisfs_file_count(void){return g_sb.file_count;}u32 aegisfs_generation(void){return g_sb.generation;}int aegisfs_sync(void){return persist_metadata();}
int aegisfs_selftest(void){if(!g_mounted||!g_writable)return AEGIS_EINVAL;vnode_t*vn=vfs_open("/persist/.pr1-selftest");if(!vn)return AEGIS_EIO;static const char data[]="aegisfs-pr1";int rc=vfs_write(vn,0,data,sizeof(data));if(rc!=(int)sizeof(data)){vfs_close(vn);return AEGIS_EIO;}char out[sizeof(data)];rc=vfs_read(vn,0,out,sizeof(out));vfs_close(vn);if(rc!=(int)sizeof(out))return AEGIS_EIO;for(u32 i=0;i<sizeof(data);i++)if(out[i]!=data[i])return AEGIS_EIO;return AEGIS_OK;}
