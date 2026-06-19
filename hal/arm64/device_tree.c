/* SPDX-License-Identifier: Proprietary */
/* AegisOS — hal/arm64/device_tree.c
 * Minimal Flattened Device Tree (FDT) parser.
 */

#include "../../kernel/include/types.h"

#define FDT_MAGIC 0xD00DFEED

typedef struct {
    u32 magic;
    u32 totalsize;
    u32 off_dt_struct;
    u32 off_dt_strings;
    u32 off_mem_rsvmap;
    u32 version;
    u32 last_comp_version;
    u32 boot_cpuid_phys;
    u32 size_dt_strings;
    u32 size_dt_struct;
} fdt_header_t;

static const fdt_header_t *fdt_hdr;

static u32 be32(u32 v) {
    return ((v & 0xFF000000) >> 24) |
           ((v & 0x00FF0000) >>  8) |
           ((v & 0x0000FF00) <<  8) |
           ((v & 0x000000FF) << 24);
}

int device_tree_init(const void *fdt) {
    fdt_hdr = (const fdt_header_t *)fdt;
    if (be32(fdt_hdr->magic) != FDT_MAGIC) return -1;
    return 0;
}

u32 device_tree_get_total_size(void) {
    if (!fdt_hdr) return 0;
    return be32(fdt_hdr->totalsize);
}
