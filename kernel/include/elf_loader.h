/* SPDX-License-Identifier: Proprietary */
#pragma once
/* AegisOS — minimal ELF64 loader contract for first userland image.
 *
 * v35/v36 scope: validate and bind /sbin/aegis-init from initramfs.
 *
 * v37 scope: copy the executable PT_LOAD segment into a kernel-owned backing
 * page for the first user image, zero the rest of the segment, and publish
 * user/kernel permission metadata for the process descriptor proof.  This is
 * still an early loader proof, not a full demand-paged VM loader.
 */

#include "types.h"

#define AEGIS_ELF_PATH_MAX 64

#define AEGIS_ELF_LOAD_READ  (1u << 0)
#define AEGIS_ELF_LOAD_WRITE (1u << 1)
#define AEGIS_ELF_LOAD_EXEC  (1u << 2)
#define AEGIS_ELF_MAX_LOAD_SEGMENTS 4

/* The ELF machine value for AArch64 per the ELF gABI. */
#define AEGIS_ELF_EM_AARCH64 183u

typedef struct aegis_elf_image_info {
    bool validated;
    bool loadable;
    char path[AEGIS_ELF_PATH_MAX];
    u64 entry;
    u64 text_vaddr;
    u64 text_filesz;
    u64 text_memsz;
    u32 text_flags;
    bool text_pt_load_copied;
    bool text_permissions_ready;
    u64 text_file_bytes_copied;
    u64 text_zero_bytes;
    u64 text_kernel_backing;
    u16 phnum;
    u16 machine;
    u16 type;
} aegis_elf_image_info_t;

void elf_loader_init(void);
int  elf_loader_validate_image(const void *image, u64 image_len,
                               const char *path,
                               aegis_elf_image_info_t *out);
int  elf_loader_load_builtin_aegis_init(aegis_elf_image_info_t *out);
int  elf_loader_load_vfs_aegis_init(aegis_elf_image_info_t *out);
int  elf_loader_selftest(void);
int  elf_loader_pt_load_selftest(void);
bool elf_loader_segments_copied(void);
bool elf_loader_permissions_ready(void);
bool elf_loader_builtin_aegis_init_ready(void);
const aegis_elf_image_info_t *elf_loader_builtin_aegis_init_info(void);
const char *elf_loader_state_name(void);
