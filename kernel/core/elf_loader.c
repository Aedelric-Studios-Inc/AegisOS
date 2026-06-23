/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/core/elf_loader.c
 * Minimal ELF64/AArch64 user image loader contract.
 *
 * v37: file-backed initramfs ELF loading now copies the executable PT_LOAD
 * segment into an aligned kernel-owned backing page, zeroes the BSS tail, and
 * publishes user/kernel permission metadata for the first process descriptor.
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

/* Keep the struct definitions local: the rest of the kernel consumes the
 * loader contract, not the raw generic ELF ABI yet.
 */
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

static aegis_elf_image_info_t builtin_info;
static bool loader_initialised;
static bool builtin_ready;
static bool pt_load_segments_copied;
static bool permissions_ready;
static u8 first_user_text_backing[PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));

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
        .e_shoff = 0,
        .e_flags = 0,
        .e_ehsize = sizeof(elf64_ehdr_t),
        .e_phentsize = sizeof(elf64_phdr_t),
        .e_phnum = 1,
        .e_shentsize = 0,
        .e_shnum = 0,
        .e_shstrndx = 0,
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

void elf_loader_init(void) {
    zero_mem(&builtin_info, sizeof(builtin_info));
    zero_mem(first_user_text_backing, sizeof(first_user_text_backing));
    loader_initialised = true;
    builtin_ready = false;
    pt_load_segments_copied = false;
    permissions_ready = false;
}

int elf_loader_validate_image(const void *image, u64 image_len,
                              const char *path,
                              aegis_elf_image_info_t *out) {
    if (!loader_initialised || !image || !out) return AEGIS_EINVAL;
    if (image_len < sizeof(elf64_ehdr_t)) return AEGIS_EINVAL;

    const elf64_ehdr_t *eh = (const elf64_ehdr_t *)image;
    if (eh->e_ident[0] != 0x7f || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F') return AEGIS_EINVAL;
    if (eh->e_ident[EI_CLASS] != ELFCLASS64) return AEGIS_EINVAL;
    if (eh->e_ident[EI_DATA] != ELFDATA2LSB) return AEGIS_EINVAL;
    if (eh->e_ident[EI_VERSION] != EV_CURRENT) return AEGIS_EINVAL;
    if (eh->e_type != ET_EXEC) return AEGIS_EINVAL;
    if (eh->e_machine != AEGIS_ELF_EM_AARCH64) return AEGIS_EINVAL;
    if (eh->e_version != EV_CURRENT) return AEGIS_EINVAL;
    if (eh->e_ehsize != sizeof(elf64_ehdr_t)) return AEGIS_EINVAL;
    if (eh->e_phentsize != sizeof(elf64_phdr_t)) return AEGIS_EINVAL;
    if (eh->e_phnum == 0 || eh->e_phnum > AEGIS_ELF_MAX_LOAD_SEGMENTS) return AEGIS_EINVAL;
    if (eh->e_phoff >= image_len) return AEGIS_EINVAL;

    u64 ph_bytes = (u64)eh->e_phentsize * (u64)eh->e_phnum;
    if (eh->e_phoff + ph_bytes > image_len || eh->e_phoff + ph_bytes < eh->e_phoff) {
        return AEGIS_EINVAL;
    }

    zero_mem(out, sizeof(*out));
    out->phnum = eh->e_phnum;
    out->machine = eh->e_machine;
    out->type = eh->e_type;
    out->entry = eh->e_entry;
    copy_text(out->path, AEGIS_ELF_PATH_MAX, path ? path : "<memory>");

    const elf64_phdr_t *ph = (const elf64_phdr_t *)((const u8 *)image + eh->e_phoff);
    for (u16 i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD) continue;
        if (ph[i].p_memsz == 0 || ph[i].p_filesz > ph[i].p_memsz) return AEGIS_EINVAL;
        if (ph[i].p_align != 0 && (ph[i].p_align & (ph[i].p_align - 1U)) != 0) return AEGIS_EINVAL;
        if ((ph[i].p_vaddr & (PAGE_SIZE - 1U)) != 0) return AEGIS_EINVAL;
        if ((ph[i].p_memsz & (PAGE_SIZE - 1U)) != 0) return AEGIS_EINVAL;
        if (ph[i].p_memsz > PAGE_SIZE) return AEGIS_EINVAL;
        if (ph[i].p_offset + ph[i].p_filesz > image_len || ph[i].p_offset + ph[i].p_filesz < ph[i].p_offset) {
            return AEGIS_EINVAL;
        }
        if (!range_contains(ph[i].p_vaddr, ph[i].p_memsz, eh->e_entry)) continue;
        if ((ph[i].p_flags & PF_X) == 0) continue;

        out->text_vaddr = ph[i].p_vaddr;
        out->text_filesz = ph[i].p_filesz;
        out->text_memsz = ph[i].p_memsz;
        out->text_flags = elf_flags_to_loader_flags(ph[i].p_flags);
        out->loadable = true;
        break;
    }

    if (!out->loadable) return AEGIS_EINVAL;
    out->validated = true;
    return AEGIS_OK;
}

static int copy_executable_pt_load(const void *image, u64 image_len,
                                   aegis_elf_image_info_t *info) {
    if (!image || !info || !info->validated || !info->loadable) return AEGIS_EINVAL;
    const elf64_ehdr_t *eh = (const elf64_ehdr_t *)image;
    const elf64_phdr_t *ph = (const elf64_phdr_t *)((const u8 *)image + eh->e_phoff);

    for (u16 i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD) continue;
        if ((ph[i].p_flags & PF_X) == 0) continue;
        if (ph[i].p_vaddr != info->text_vaddr || ph[i].p_memsz != info->text_memsz) continue;
        if (ph[i].p_filesz > PAGE_SIZE || ph[i].p_memsz > PAGE_SIZE) return AEGIS_EINVAL;
        if (ph[i].p_offset + ph[i].p_filesz > image_len || ph[i].p_offset + ph[i].p_filesz < ph[i].p_offset) {
            return AEGIS_EINVAL;
        }

        zero_mem(first_user_text_backing, PAGE_SIZE);
        copy_mem(first_user_text_backing, (const u8 *)image + ph[i].p_offset, ph[i].p_filesz);

        info->text_pt_load_copied = true;
        info->text_permissions_ready = true;
        info->text_file_bytes_copied = ph[i].p_filesz;
        info->text_zero_bytes = ph[i].p_memsz - ph[i].p_filesz;
        info->text_kernel_backing = (u64)(uptr)first_user_text_backing;
        pt_load_segments_copied = true;
        permissions_ready = true;
        return AEGIS_OK;
    }

    return AEGIS_EINVAL;
}

int elf_loader_load_builtin_aegis_init(aegis_elf_image_info_t *out) {
    if (!loader_initialised) return AEGIS_EINVAL;

    aegis_elf_image_info_t tmp;
    int rc = elf_loader_validate_image(&builtin_aegis_init_elf,
                                       sizeof(builtin_aegis_init_elf),
                                       "/sbin/aegis-init",
                                       &tmp);
    if (rc != AEGIS_OK) return rc;
    rc = copy_executable_pt_load(&builtin_aegis_init_elf, sizeof(builtin_aegis_init_elf), &tmp);
    if (rc != AEGIS_OK) return rc;
    copy_info(&builtin_info, &tmp);
    builtin_ready = true;
    if (out) copy_info(out, &builtin_info);
    return AEGIS_OK;
}

int elf_loader_load_vfs_aegis_init(aegis_elf_image_info_t *out) {
    if (!loader_initialised) return AEGIS_EINVAL;

    /* v36.1/v37: keep copied ELF bytes naturally aligned before casting to ELF64 headers. */
    static u8 image_buf[512] __attribute__((aligned(16)));
    vnode_t *vn = vfs_open("/sbin/aegis-init");
    if (!vn) return AEGIS_ENOENT;

    int n = vfs_read(vn, 0, image_buf, sizeof(image_buf));
    vfs_close(vn);
    if (n <= 0) return AEGIS_EINVAL;

    aegis_elf_image_info_t tmp;
    int rc = elf_loader_validate_image(image_buf, (u64)n, "/sbin/aegis-init", &tmp);
    if (rc != AEGIS_OK) return rc;
    rc = copy_executable_pt_load(image_buf, (u64)n, &tmp);
    if (rc != AEGIS_OK) return rc;

    copy_info(&builtin_info, &tmp);
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
    if (builtin_info.text_memsz == 0 || (builtin_info.text_memsz & (PAGE_SIZE - 1U)) != 0) {
        return AEGIS_EINVAL;
    }
    if ((builtin_info.text_flags & AEGIS_ELF_LOAD_EXEC) == 0) return AEGIS_EINVAL;
    return AEGIS_OK;
}

int elf_loader_pt_load_selftest(void) {
    if (!loader_initialised || !builtin_ready) return AEGIS_EINVAL;
    if (!pt_load_segments_copied || !permissions_ready) return AEGIS_EINVAL;
    if (!builtin_info.text_pt_load_copied || !builtin_info.text_permissions_ready) return AEGIS_EINVAL;
    if (builtin_info.text_kernel_backing != (u64)(uptr)first_user_text_backing) return AEGIS_EINVAL;
    if (builtin_info.text_file_bytes_copied == 0) return AEGIS_EINVAL;
    if (builtin_info.text_file_bytes_copied > builtin_info.text_memsz) return AEGIS_EINVAL;
    if (builtin_info.text_zero_bytes != builtin_info.text_memsz - builtin_info.text_file_bytes_copied) return AEGIS_EINVAL;
    if ((builtin_info.text_flags & AEGIS_ELF_LOAD_READ) == 0) return AEGIS_EINVAL;
    if ((builtin_info.text_flags & AEGIS_ELF_LOAD_EXEC) == 0) return AEGIS_EINVAL;
    if ((builtin_info.text_flags & AEGIS_ELF_LOAD_WRITE) != 0) return AEGIS_EINVAL;
    if (first_user_text_backing[0] == 0 && builtin_info.text_file_bytes_copied != 0) return AEGIS_EINVAL;
    if (first_user_text_backing[builtin_info.text_file_bytes_copied] != 0) return AEGIS_EINVAL;
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

const char *elf_loader_state_name(void) {
    if (!loader_initialised) return "uninitialised";
    if (!builtin_ready) return "initialised";
    if (!pt_load_segments_copied) return "validated-no-segments";
    if (!permissions_ready) return "segments-copied-no-permissions";
    return "file-backed-aegis-init-ptload-ready";
}
