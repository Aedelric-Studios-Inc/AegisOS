/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/core/elf_loader.c
 * Native ELF64/AArch64 runtime loader.
 *
 * v57 generalises the v56 one-image proof into a bounded runtime-image pool.
 * Every native process receives distinct executable backing and a distinct EL0
 * stack.  The loader still supports one RX PT_LOAD page per image; demand
 * paging, relocations, writable ELF segments, and per-process page tables are
 * later release gates.
 */

#include "elf_loader.h"
#include "userland.h"
#include "memory.h"
#include "vfs.h"

#define EI_NIDENT 16
#define EI_CLASS  4
#define EI_DATA   5
#define EI_VERSION 6

#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define EV_CURRENT 1
#define ET_EXEC 2
#define PT_LOAD 1
#define PF_X 1
#define PF_W 2
#define PF_R 4

#define AEGIS_ELF_IMAGE_BUF_MAX (512U * 1024U)

typedef struct elf64_ehdr {
    u8  e_ident[EI_NIDENT];
    u16 e_type;
    u16 e_machine;
    u32 e_version;
    u64 e_entry;
    u64 e_phoff;
    u64 e_shoff;
    u32 e_flags;
    u16 e_ehsize;
    u16 e_phentsize;
    u16 e_phnum;
    u16 e_shentsize;
    u16 e_shnum;
    u16 e_shstrndx;
} elf64_ehdr_t;

typedef struct elf64_phdr {
    u32 p_type;
    u32 p_flags;
    u64 p_offset;
    u64 p_vaddr;
    u64 p_paddr;
    u64 p_filesz;
    u64 p_memsz;
    u64 p_align;
} elf64_phdr_t;

typedef struct builtin_aegis_init_elf {
    elf64_ehdr_t ehdr;
    elf64_phdr_t text;
    u8 payload[16];
} builtin_aegis_init_elf_t;

typedef struct aegis_elf_runtime_segment {
    u64 base;
    u64 size;
    u32 flags;
} aegis_elf_runtime_segment_t;

typedef struct aegis_elf_runtime_slot {
    bool used;
    aegis_elf_image_info_t info;
    u8 image_backing[AEGIS_ELF_RUNTIME_IMAGE_MAX] __attribute__((aligned(PAGE_SIZE)));
    u8 stack_backing[AEGIS_USER_STACK_SIZE] __attribute__((aligned(16)));
    aegis_elf_runtime_segment_t segments[AEGIS_ELF_MAX_LOAD_SEGMENTS];
    u32 segment_count;
} aegis_elf_runtime_slot_t;

static aegis_elf_runtime_slot_t runtime_slots[AEGIS_ELF_RUNTIME_MAX];
static aegis_elf_image_info_t builtin_info;
static bool loader_initialised;
static bool builtin_ready;
static bool pt_load_segments_copied;
static bool permissions_ready;
static u32 builtin_runtime_slot;

static void zero_mem(void *ptr, u64 len) {
    u8 *p = (u8 *)ptr;
    for (u64 i = 0; i < len; i++) p[i] = 0;
}

static void copy_mem(void *dst, const void *src, u64 len) {
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    for (u64 i = 0; i < len; i++) d[i] = s[i];
}

static void copy_info(aegis_elf_image_info_t *dst, const aegis_elf_image_info_t *src) {
    copy_mem(dst, src, sizeof(*dst));
}

static void copy_text(char *dst, u32 dst_len, const char *src) {
    u32 i = 0;
    if (!dst || dst_len == 0) return;
    if (!src) src = "";
    while (src[i] && i < dst_len - 1U) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static bool range_contains(u64 base, u64 size, u64 addr) {
    if (size == 0) return false;
    u64 end = base + size;
    if (end <= base) return false;
    return addr >= base && addr < end;
}

static bool full_range_inside(u64 ptr, u64 len, u64 base, u64 size) {
    if (ptr == 0 || len == 0 || size == 0) return false;
    u64 end = ptr + len;
    u64 region_end = base + size;
    if (end <= ptr || region_end <= base) return false;
    return ptr >= base && end <= region_end;
}

static void sync_executable_range(void *start, u64 len) {
    if (!start || len == 0) return;

    u64 sctlr = 0;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    const bool dcache_enabled = (sctlr & (1ULL << 2)) != 0;
    const bool icache_enabled = (sctlr & (1ULL << 12)) != 0;
    if (!dcache_enabled && !icache_enabled) {
        __asm__ volatile("dsb sy\n\tisb" ::: "memory");
        return;
    }

    u64 ctr = 0;
    __asm__ volatile("mrs %0, ctr_el0" : "=r"(ctr));
    uptr dline = (uptr)4U << ((ctr >> 16) & 0xFULL);
    uptr iline = (uptr)4U << (ctr & 0xFULL);

    if (dcache_enabled) {
        uptr first = ((uptr)start) & ~(dline - 1U);
        uptr end = ((uptr)start + (uptr)len + dline - 1U) & ~(dline - 1U);
        for (uptr addr = first; addr < end; addr += dline) {
            __asm__ volatile("dc cvau, %0" :: "r"(addr) : "memory");
        }
        __asm__ volatile("dsb ish" ::: "memory");
    }

    if (icache_enabled) {
        uptr first = ((uptr)start) & ~(iline - 1U);
        uptr end = ((uptr)start + (uptr)len + iline - 1U) & ~(iline - 1U);
        for (uptr addr = first; addr < end; addr += iline) {
            __asm__ volatile("ic ivau, %0" :: "r"(addr) : "memory");
        }
        __asm__ volatile("dsb ish" ::: "memory");
    }
    __asm__ volatile("isb" ::: "memory");
}

static u32 elf_flags_to_loader_flags(u32 flags) {
    u32 out = 0;
    if (flags & PF_R) out |= AEGIS_ELF_LOAD_READ;
    if (flags & PF_W) out |= AEGIS_ELF_LOAD_WRITE;
    if (flags & PF_X) out |= AEGIS_ELF_LOAD_EXEC;
    return out;
}

static const builtin_aegis_init_elf_t builtin_aegis_init_elf = {
    .ehdr = {
        .e_ident = {0x7f, 'E', 'L', 'F', ELFCLASS64, ELFDATA2LSB, EV_CURRENT, 0},
        .e_type = ET_EXEC,
        .e_machine = AEGIS_ELF_EM_AARCH64,
        .e_version = EV_CURRENT,
        .e_entry = AEGIS_USER_TEXT_BASE,
        .e_phoff = sizeof(elf64_ehdr_t),
        .e_ehsize = sizeof(elf64_ehdr_t),
        .e_phentsize = sizeof(elf64_phdr_t),
        .e_phnum = 1,
    },
    .text = {
        .p_type = PT_LOAD,
        .p_flags = PF_R | PF_X,
        .p_offset = sizeof(elf64_ehdr_t) + sizeof(elf64_phdr_t),
        .p_vaddr = AEGIS_USER_TEXT_BASE,
        .p_paddr = AEGIS_USER_TEXT_BASE,
        .p_filesz = sizeof(((builtin_aegis_init_elf_t *)0)->payload),
        .p_memsz = PAGE_SIZE,
        .p_align = PAGE_SIZE,
    },
    .payload = {0xa9, 0x37, 0x00, 0x00, 0x54, 0x37, 0x00, 0x00,
                0x50, 0x54, 0x4c, 0x44, 0x50, 0x45, 0x52, 0x4d},
};

static aegis_elf_runtime_slot_t *runtime_slot_from_id(u32 id) {
    if (id == 0 || id > AEGIS_ELF_RUNTIME_MAX) return NULL;
    aegis_elf_runtime_slot_t *slot = &runtime_slots[id - 1U];
    return slot->used ? slot : NULL;
}

static aegis_elf_runtime_slot_t *runtime_slot_allocate(u32 *out_id) {
    for (u32 i = 0; i < AEGIS_ELF_RUNTIME_MAX; i++) {
        if (runtime_slots[i].used) continue;
        runtime_slots[i].used = true;
        zero_mem(&runtime_slots[i].info, sizeof(runtime_slots[i].info));
        zero_mem(runtime_slots[i].image_backing, sizeof(runtime_slots[i].image_backing));
        zero_mem(runtime_slots[i].segments, sizeof(runtime_slots[i].segments));
        runtime_slots[i].segment_count = 0;
        zero_mem(runtime_slots[i].stack_backing, sizeof(runtime_slots[i].stack_backing));
        if (out_id) *out_id = i + 1U;
        return &runtime_slots[i];
    }
    return NULL;
}

void elf_loader_init(void) {
    zero_mem(&builtin_info, sizeof(builtin_info));
    for (u32 i = 0; i < AEGIS_ELF_RUNTIME_MAX; i++) {
        runtime_slots[i].used = false;
        zero_mem(&runtime_slots[i].info, sizeof(runtime_slots[i].info));
    }
    loader_initialised = true;
    builtin_ready = false;
    pt_load_segments_copied = false;
    permissions_ready = false;
    builtin_runtime_slot = 0;
}

int elf_loader_validate_image(const void *image, u64 image_len,
                              const char *path,
                              aegis_elf_image_info_t *out) {
    if (!loader_initialised || !image || !out || image_len < sizeof(elf64_ehdr_t)) return AEGIS_EINVAL;
    const elf64_ehdr_t *eh = (const elf64_ehdr_t *)image;
    if (eh->e_ident[0] != 0x7f || eh->e_ident[1] != 'E' || eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F') return AEGIS_EINVAL;
    if (eh->e_ident[EI_CLASS] != ELFCLASS64 || eh->e_ident[EI_DATA] != ELFDATA2LSB || eh->e_ident[EI_VERSION] != EV_CURRENT) return AEGIS_EINVAL;
    if (eh->e_type != ET_EXEC || eh->e_machine != AEGIS_ELF_EM_AARCH64 || eh->e_version != EV_CURRENT) return AEGIS_EINVAL;
    if (eh->e_ehsize != sizeof(elf64_ehdr_t) || eh->e_phentsize != sizeof(elf64_phdr_t)) return AEGIS_EINVAL;
    if (eh->e_phnum == 0 || eh->e_phnum > AEGIS_ELF_MAX_LOAD_SEGMENTS || eh->e_phoff >= image_len) return AEGIS_EINVAL;
    u64 ph_bytes=(u64)eh->e_phentsize*(u64)eh->e_phnum;
    if(eh->e_phoff+ph_bytes>image_len||eh->e_phoff+ph_bytes<eh->e_phoff)return AEGIS_EINVAL;

    zero_mem(out,sizeof(*out));out->phnum=eh->e_phnum;out->machine=eh->e_machine;out->type=eh->e_type;out->entry=eh->e_entry;
    copy_text(out->path,AEGIS_ELF_PATH_MAX,path?path:"<memory>");
    const elf64_phdr_t *ph=(const elf64_phdr_t*)((const u8*)image+eh->e_phoff);
    u64 min_vaddr=~0ULL,max_vaddr=0;bool entry_exec=false;u32 loads=0;
    for(u16 i=0;i<eh->e_phnum;i++){
        if(ph[i].p_type!=PT_LOAD)continue;
        if(++loads>AEGIS_ELF_MAX_LOAD_SEGMENTS||ph[i].p_memsz==0||ph[i].p_filesz>ph[i].p_memsz)return AEGIS_EINVAL;
        if(ph[i].p_align&&((ph[i].p_align&(ph[i].p_align-1U))!=0))return AEGIS_EINVAL;
        if(ph[i].p_offset+ph[i].p_filesz>image_len||ph[i].p_offset+ph[i].p_filesz<ph[i].p_offset)return AEGIS_EINVAL;
        if((ph[i].p_flags&(PF_W|PF_X))==(PF_W|PF_X))return AEGIS_EPERM;
        u64 seg_start=ph[i].p_vaddr&~(u64)(PAGE_SIZE-1U);
        u64 seg_end=(ph[i].p_vaddr+ph[i].p_memsz+PAGE_SIZE-1U)&~(u64)(PAGE_SIZE-1U);
        if(seg_end<=seg_start)return AEGIS_EINVAL;
        if(seg_start<min_vaddr)min_vaddr=seg_start;if(seg_end>max_vaddr)max_vaddr=seg_end;
        if(range_contains(ph[i].p_vaddr,ph[i].p_memsz,eh->e_entry)&&(ph[i].p_flags&PF_X)){
            entry_exec=true;out->text_vaddr=ph[i].p_vaddr;out->text_filesz=ph[i].p_filesz;out->text_memsz=ph[i].p_memsz;out->text_flags=elf_flags_to_loader_flags(ph[i].p_flags);
        }
    }
    if(!loads||!entry_exec||min_vaddr==~0ULL||max_vaddr-min_vaddr>AEGIS_ELF_RUNTIME_IMAGE_MAX)return AEGIS_EINVAL;
    out->runtime_image_size=max_vaddr-min_vaddr;out->load_segment_count=loads;out->loadable=true;out->validated=true;return AEGIS_OK;
}

static int copy_executable_pt_load(const void *image, u64 image_len,
                                   aegis_elf_runtime_slot_t *slot,
                                   u32 slot_id,
                                   aegis_elf_image_info_t *info) {
    if(!image||!slot||!info||!info->validated||!info->loadable)return AEGIS_EINVAL;
    const elf64_ehdr_t*eh=(const elf64_ehdr_t*)image;
    const elf64_phdr_t*ph=(const elf64_phdr_t*)((const u8*)image+eh->e_phoff);
    u64 min_vaddr=~0ULL,max_vaddr=0;
    for(u16 i=0;i<eh->e_phnum;i++)if(ph[i].p_type==PT_LOAD){u64 a=ph[i].p_vaddr&~(u64)(PAGE_SIZE-1U);u64 b=(ph[i].p_vaddr+ph[i].p_memsz+PAGE_SIZE-1U)&~(u64)(PAGE_SIZE-1U);if(a<min_vaddr)min_vaddr=a;if(b>max_vaddr)max_vaddr=b;}
    if(min_vaddr==~0ULL||max_vaddr-min_vaddr>sizeof(slot->image_backing))return AEGIS_ENOMEM;
    zero_mem(slot->image_backing,sizeof(slot->image_backing));zero_mem(slot->segments,sizeof(slot->segments));slot->segment_count=0;
    u64 copied=0,zeroed=0;
    for(u16 i=0;i<eh->e_phnum;i++){
        if(ph[i].p_type!=PT_LOAD)continue;
        if(ph[i].p_offset+ph[i].p_filesz>image_len)return AEGIS_EINVAL;
        u64 off=ph[i].p_vaddr-min_vaddr;if(off+ph[i].p_memsz>sizeof(slot->image_backing))return AEGIS_ENOMEM;
        copy_mem(slot->image_backing+off,(const u8*)image+ph[i].p_offset,ph[i].p_filesz);
        aegis_elf_runtime_segment_t*seg=&slot->segments[slot->segment_count++];seg->base=(u64)(uptr)(slot->image_backing+off);seg->size=ph[i].p_memsz;seg->flags=elf_flags_to_loader_flags(ph[i].p_flags);
        copied+=ph[i].p_filesz;zeroed+=ph[i].p_memsz-ph[i].p_filesz;
        if(ph[i].p_flags&PF_X)sync_executable_range(slot->image_backing+off,ph[i].p_memsz);
    }
    info->text_pt_load_copied=true;info->text_permissions_ready=true;info->text_file_bytes_copied=copied;info->text_zero_bytes=zeroed;
    info->runtime_image_base=(u64)(uptr)slot->image_backing;info->runtime_image_size=max_vaddr-min_vaddr;
    info->text_kernel_backing=info->runtime_image_base; /* compatibility: complete readable image span */
    info->text_memsz=info->runtime_image_size;
    info->runtime_entry=info->runtime_image_base+(info->entry-min_vaddr);
    info->runtime_stack_base=(u64)(uptr)slot->stack_backing;info->runtime_stack_size=sizeof(slot->stack_backing);info->runtime_stack_top=(info->runtime_stack_base+info->runtime_stack_size)&~0xFULL;info->runtime_slot=slot_id;
    copy_info(&slot->info,info);pt_load_segments_copied=true;permissions_ready=true;return AEGIS_OK;
}

static int load_image_into_runtime(const void *image, u64 image_len,
                                   const char *path,
                                   aegis_elf_image_info_t *out) {
    aegis_elf_image_info_t tmp;
    int rc = elf_loader_validate_image(image, image_len, path, &tmp);
    if (rc != AEGIS_OK) return rc;

    u32 slot_id = 0;
    aegis_elf_runtime_slot_t *slot = runtime_slot_allocate(&slot_id);
    if (!slot) return AEGIS_ENOMEM;

    rc = copy_executable_pt_load(image, image_len, slot, slot_id, &tmp);
    if (rc != AEGIS_OK) {
        slot->used = false;
        return rc;
    }
    if (out) copy_info(out, &tmp);
    return AEGIS_OK;
}

int elf_loader_load_builtin_aegis_init(aegis_elf_image_info_t *out) {
    if (!loader_initialised) return AEGIS_EINVAL;
    aegis_elf_image_info_t tmp;
    int rc = load_image_into_runtime(&builtin_aegis_init_elf,
                                     sizeof(builtin_aegis_init_elf),
                                     "/sbin/aegis-init",
                                     &tmp);
    if (rc != AEGIS_OK) return rc;
    copy_info(&builtin_info, &tmp);
    builtin_runtime_slot = tmp.runtime_slot;
    builtin_ready = true;
    if (out) copy_info(out, &builtin_info);
    return AEGIS_OK;
}

int elf_loader_load_vfs_path(const char *path, aegis_elf_image_info_t *out) {
    if (!loader_initialised || !path || !out) return AEGIS_EINVAL;

    static u8 image_buf[AEGIS_ELF_IMAGE_BUF_MAX] __attribute__((aligned(16)));
    vnode_t *vn = vfs_open(path);
    if (!vn) return AEGIS_ENOENT;
    int n = vfs_read(vn, 0, image_buf, sizeof(image_buf));
    vfs_close(vn);
    if (n <= 0) return AEGIS_EINVAL;
    if ((u64)n >= sizeof(image_buf)) return AEGIS_ENOMEM;

    return load_image_into_runtime(image_buf, (u64)n, path, out);
}


int elf_loader_release_runtime_slot(u32 runtime_slot) {
    if (!loader_initialised || runtime_slot == 0 || runtime_slot > AEGIS_ELF_RUNTIME_MAX) {
        return AEGIS_EINVAL;
    }
    if (runtime_slot == builtin_runtime_slot) {
        /* PID 1 owns the bootstrap slot for the lifetime of the kernel. */
        return AEGIS_EPERM;
    }

    aegis_elf_runtime_slot_t *slot = &runtime_slots[runtime_slot - 1U];
    if (!slot->used) return AEGIS_ENOENT;

    zero_mem(slot->image_backing, sizeof(slot->image_backing));
    zero_mem(slot->segments, sizeof(slot->segments));
    slot->segment_count = 0;
    zero_mem(slot->stack_backing, sizeof(slot->stack_backing));
    zero_mem(&slot->info, sizeof(slot->info));
    slot->used = false;
    return AEGIS_OK;
}

int elf_loader_load_vfs_aegis_init(aegis_elf_image_info_t *out) {
    if (!loader_initialised) return AEGIS_EINVAL;
    aegis_elf_image_info_t tmp;
    int rc = elf_loader_load_vfs_path("/sbin/aegis-init", &tmp);
    if (rc != AEGIS_OK) return rc;
    copy_info(&builtin_info, &tmp);
    builtin_runtime_slot = tmp.runtime_slot;
    builtin_ready = true;
    if (out) copy_info(out, &builtin_info);
    return AEGIS_OK;
}

int elf_loader_selftest(void) {
    if (!loader_initialised || !builtin_ready) return AEGIS_EINVAL;
    if (!builtin_info.validated || !builtin_info.loadable) return AEGIS_EINVAL;
    if (builtin_info.machine != AEGIS_ELF_EM_AARCH64) return AEGIS_EINVAL;
    if (builtin_info.entry != AEGIS_USER_TEXT_BASE) return AEGIS_EINVAL;
    if (builtin_info.text_vaddr != AEGIS_USER_TEXT_BASE) return AEGIS_EINVAL;
    if (builtin_info.text_memsz == 0 || (builtin_info.text_memsz & (PAGE_SIZE - 1U)) != 0) return AEGIS_EINVAL;
    if ((builtin_info.text_flags & AEGIS_ELF_LOAD_EXEC) == 0) return AEGIS_EINVAL;
    return runtime_slot_from_id(builtin_runtime_slot) ? AEGIS_OK : AEGIS_EINVAL;
}

int elf_loader_pt_load_selftest(void) {
    if (!loader_initialised || !builtin_ready) return AEGIS_EINVAL;
    aegis_elf_runtime_slot_t *slot = runtime_slot_from_id(builtin_runtime_slot);
    if (!slot || !pt_load_segments_copied || !permissions_ready) return AEGIS_EINVAL;
    if (!builtin_info.text_pt_load_copied || !builtin_info.text_permissions_ready) return AEGIS_EINVAL;
    if (builtin_info.text_kernel_backing != (u64)(uptr)slot->image_backing) return AEGIS_EINVAL;
    if (builtin_info.runtime_entry < builtin_info.text_kernel_backing) return AEGIS_EINVAL;
    if (builtin_info.runtime_entry >= builtin_info.text_kernel_backing + builtin_info.text_memsz) return AEGIS_EINVAL;
    if (builtin_info.runtime_stack_base != (u64)(uptr)slot->stack_backing) return AEGIS_EINVAL;
    if (builtin_info.runtime_stack_size != sizeof(slot->stack_backing)) return AEGIS_EINVAL;
    if ((builtin_info.runtime_stack_top & 0xFULL) != 0) return AEGIS_EINVAL;
    if (builtin_info.text_file_bytes_copied == 0 || builtin_info.text_file_bytes_copied > builtin_info.text_memsz) return AEGIS_EINVAL;
    if (builtin_info.text_zero_bytes != builtin_info.text_memsz - builtin_info.text_file_bytes_copied) return AEGIS_EINVAL;
    if ((builtin_info.text_flags & AEGIS_ELF_LOAD_READ) == 0) return AEGIS_EINVAL;
    if ((builtin_info.text_flags & AEGIS_ELF_LOAD_EXEC) == 0) return AEGIS_EINVAL;
    if ((builtin_info.text_flags & AEGIS_ELF_LOAD_WRITE) != 0) return AEGIS_EINVAL;
    if (slot->image_backing[0] == 0 && builtin_info.text_file_bytes_copied != 0) return AEGIS_EINVAL;
    if (slot->image_backing[builtin_info.text_file_bytes_copied] != 0) return AEGIS_EINVAL;
    return AEGIS_OK;
}

bool elf_loader_segments_copied(void) {
    return loader_initialised && builtin_ready && pt_load_segments_copied && builtin_info.text_pt_load_copied;
}

bool elf_loader_permissions_ready(void) {
    return elf_loader_segments_copied() && permissions_ready && builtin_info.text_permissions_ready;
}

bool elf_loader_builtin_aegis_init_ready(void) {
    return loader_initialised && builtin_ready;
}

const aegis_elf_image_info_t *elf_loader_builtin_aegis_init_info(void) {
    return builtin_ready ? &builtin_info : NULL;
}

u64 elf_loader_runtime_entry(void) {
    return builtin_ready ? builtin_info.runtime_entry : 0;
}

u64 elf_loader_runtime_stack_base(void) {
    return builtin_ready ? builtin_info.runtime_stack_base : 0;
}

u64 elf_loader_runtime_stack_size(void) {
    return builtin_ready ? builtin_info.runtime_stack_size : 0;
}

u64 elf_loader_runtime_stack_top(void) {
    return builtin_ready ? builtin_info.runtime_stack_top : 0;
}

bool elf_loader_slot_read_range_ok(u32 runtime_slot,u64 ptr,u64 len){
    aegis_elf_runtime_slot_t*slot=runtime_slot_from_id(runtime_slot);if(!slot)return false;
    if(full_range_inside(ptr,len,slot->info.runtime_stack_base,slot->info.runtime_stack_size))return true;
    for(u32 i=0;i<slot->segment_count;i++)if((slot->segments[i].flags&AEGIS_ELF_LOAD_READ)&&full_range_inside(ptr,len,slot->segments[i].base,slot->segments[i].size))return true;
    return false;
}
bool elf_loader_slot_write_range_ok(u32 runtime_slot,u64 ptr,u64 len){
    aegis_elf_runtime_slot_t*slot=runtime_slot_from_id(runtime_slot);if(!slot)return false;
    if(full_range_inside(ptr,len,slot->info.runtime_stack_base,slot->info.runtime_stack_size))return true;
    for(u32 i=0;i<slot->segment_count;i++)if((slot->segments[i].flags&AEGIS_ELF_LOAD_WRITE)&&full_range_inside(ptr,len,slot->segments[i].base,slot->segments[i].size))return true;
    return false;
}
bool elf_loader_runtime_read_range_ok(u64 ptr,u64 len){return builtin_ready&&elf_loader_slot_read_range_ok(builtin_runtime_slot,ptr,len);}
bool elf_loader_runtime_write_range_ok(u64 ptr,u64 len){return builtin_ready&&elf_loader_slot_write_range_ok(builtin_runtime_slot,ptr,len);}

u32 elf_loader_runtime_count(void) {
    u32 count = 0;
    for (u32 i = 0; i < AEGIS_ELF_RUNTIME_MAX; i++) {
        if (runtime_slots[i].used) count++;
    }
    return count;
}

const char *elf_loader_state_name(void) {
    if (!loader_initialised) return "uninitialised";
    if (!builtin_ready) return "initialised";
    if (!pt_load_segments_copied) return "validated-no-segments";
    if (!permissions_ready) return "segments-copied-no-permissions";
    return builtin_info.runtime_entry ? "v58-native-lifecycle-runtime-ready" : "file-backed-aegis-init-ptload-ready";
}
