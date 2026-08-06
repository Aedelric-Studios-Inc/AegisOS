/* SPDX-License-Identifier: Proprietary */
/* AegisOS — hal/arm64/device_tree.c
 * Bounds-checked Flattened Device Tree parser used during early AArch64 boot.
 */

#include "../include/device_tree.h"

#define FDT_MAGIC       0xD00DFEEDU
#define FDT_BEGIN_NODE  0x00000001U
#define FDT_END_NODE    0x00000002U
#define FDT_PROP        0x00000003U
#define FDT_NOP         0x00000004U
#define FDT_END         0x00000009U
#define FDT_MAX_DEPTH   32U

typedef struct fdt_header {
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

typedef struct fdt_node_state {
    bool is_memory;
    bool is_chosen;
    bool is_virtio_mmio;
    bool is_pl011;
    bool is_gic;
    bool is_pl031;
    bool is_pci_ecam;
    bool have_reg;
    u64 reg_base;
    u64 reg_size;
    u32 irq;
    u8 bus_start;
    u8 bus_end;
} fdt_node_state_t;

static const fdt_header_t *g_hdr;
static const u8 *g_blob;
static aegis_dtb_platform_t g_platform;

static u32 be32_load(const void *p) {
    const u8 *b = (const u8 *)p;
    return ((u32)b[0] << 24) | ((u32)b[1] << 16) | ((u32)b[2] << 8) | (u32)b[3];
}

static u64 be_cells(const u8 *p, u32 cells) {
    u64 value = 0;
    if (cells > 2U) cells = 2U;
    for (u32 i = 0; i < cells; i++) value = (value << 32) | be32_load(p + i * 4U);
    return value;
}

static u32 align4(u32 value) { return (value + 3U) & ~3U; }

static void zero_bytes(void *ptr, u64 len) {
    u8 *p = (u8 *)ptr;
    for (u64 i = 0; i < len; i++) p[i] = 0;
}

static bool bytes_equal(const char *a, const char *b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a++ != *b++) return false;
    }
    return *a == '\0' && *b == '\0';
}

static bool compatible_has(const u8 *value, u32 len, const char *needle) {
    u32 off = 0;
    while (off < len) {
        const char *entry = (const char *)(value + off);
        u32 n = 0;
        while (off + n < len && entry[n]) n++;
        if (off + n >= len) return false;
        if (bytes_equal(entry, needle)) return true;
        off += n + 1U;
    }
    return false;
}

static void copy_string(char *dst, u32 cap, const u8 *src, u32 len) {
    if (!dst || cap == 0U) return;
    u32 n = 0;
    while (n + 1U < cap && n < len && src[n]) {
        dst[n] = (char)src[n];
        n++;
    }
    dst[n] = '\0';
}

static bool range_valid(u32 off, u32 len, u32 total) {
    return off <= total && len <= total - off;
}

static void commit_node(const fdt_node_state_t *node) {
    if (!node) return;
    if (node->is_memory && node->have_reg && node->reg_size != 0U && g_platform.memory_size == 0U) {
        g_platform.memory_base = node->reg_base;
        g_platform.memory_size = node->reg_size;
    }
    if (node->is_pl011 && node->have_reg && g_platform.uart_base == 0U) {
        g_platform.uart_base = node->reg_base;
    }
    if (node->is_gic && node->have_reg && g_platform.gic_dist_base == 0U) {
        g_platform.gic_dist_base = node->reg_base;
        g_platform.gic_cpu_base = node->reg_base + 0x10000U;
    }
    if (node->is_pl031 && node->have_reg && g_platform.rtc_base == 0U) {
        g_platform.rtc_base = node->reg_base;
    }
    if (node->is_pci_ecam && node->have_reg && g_platform.pci_ecam_base == 0U) {
        g_platform.pci_ecam_base = node->reg_base;
        g_platform.pci_ecam_size = node->reg_size;
        g_platform.pci_bus_start = node->bus_start;
        g_platform.pci_bus_end = node->bus_end;
    }
    if (node->is_virtio_mmio && node->have_reg && g_platform.virtio_count < AEGIS_DTB_VIRTIO_MAX) {
        aegis_dtb_mmio_device_t *dev = &g_platform.virtio[g_platform.virtio_count++];
        dev->base = node->reg_base;
        dev->size = node->reg_size;
        dev->irq = node->irq;
        dev->present = true;
    }
}

int device_tree_init(const void *fdt) {
    zero_bytes(&g_platform, sizeof(g_platform));
    g_hdr = NULL;
    g_blob = NULL;
    if (!fdt) return AEGIS_EINVAL;

    const u8 *blob = (const u8 *)fdt;
    if (be32_load(blob) != FDT_MAGIC) return AEGIS_EINVAL;
    u32 total = be32_load(blob + 4U);
    if (total < sizeof(fdt_header_t) || total > (16U * 1024U * 1024U)) return AEGIS_EINVAL;

    u32 struct_off = be32_load(blob + 8U);
    u32 strings_off = be32_load(blob + 12U);
    u32 strings_len = be32_load(blob + 32U);
    u32 struct_len = be32_load(blob + 36U);
    if (!range_valid(struct_off, struct_len, total) || !range_valid(strings_off, strings_len, total)) return AEGIS_EINVAL;

    g_hdr = (const fdt_header_t *)fdt;
    g_blob = blob;
    g_platform.total_size = total;

    const u8 *sp = blob + struct_off;
    const u8 *send = sp + struct_len;
    const u8 *strings = blob + strings_off;
    fdt_node_state_t stack[FDT_MAX_DEPTH];
    zero_bytes(stack, sizeof(stack));
    u32 depth = 0;
    u32 root_addr_cells = 2U;
    u32 root_size_cells = 2U;

    while (sp + 4U <= send) {
        u32 token = be32_load(sp);
        sp += 4U;
        if (token == FDT_BEGIN_NODE) {
            if (depth >= FDT_MAX_DEPTH) return AEGIS_ENOMEM;
            zero_bytes(&stack[depth], sizeof(stack[depth]));
            const u8 *name = sp;
            while (sp < send && *sp) sp++;
            if (sp >= send) return AEGIS_EINVAL;
            sp++;
            uptr aligned = ((uptr)sp + 3U) & ~(uptr)3U;
            sp = (const u8 *)aligned;
            if (sp > send) return AEGIS_EINVAL;
            stack[depth].is_chosen = (depth == 1U && bytes_equal((const char *)name, "chosen"));
            depth++;
        } else if (token == FDT_END_NODE) {
            if (depth == 0U) return AEGIS_EINVAL;
            depth--;
            commit_node(&stack[depth]);
        } else if (token == FDT_PROP) {
            if (depth == 0U || sp + 8U > send) return AEGIS_EINVAL;
            u32 len = be32_load(sp);
            u32 nameoff = be32_load(sp + 4U);
            sp += 8U;
            if (nameoff >= strings_len || sp + align4(len) > send) return AEGIS_EINVAL;
            const char *prop = (const char *)(strings + nameoff);
            const u8 *value = sp;
            fdt_node_state_t *node = &stack[depth - 1U];

            if (depth == 1U && bytes_equal(prop, "#address-cells") && len >= 4U) root_addr_cells = be32_load(value);
            else if (depth == 1U && bytes_equal(prop, "#size-cells") && len >= 4U) root_size_cells = be32_load(value);
            else if (bytes_equal(prop, "device_type") && len >= 7U && bytes_equal((const char *)value, "memory")) node->is_memory = true;
            else if (bytes_equal(prop, "compatible")) {
                node->is_virtio_mmio = compatible_has(value, len, "virtio,mmio");
                node->is_pl011 = compatible_has(value, len, "arm,pl011");
                node->is_gic = compatible_has(value, len, "arm,cortex-a15-gic") || compatible_has(value, len, "arm,gic-400");
                node->is_pl031 = compatible_has(value, len, "arm,pl031");
                node->is_pci_ecam = compatible_has(value, len, "pci-host-ecam-generic");
            } else if (bytes_equal(prop, "reg")) {
                u32 need_cells = root_addr_cells + root_size_cells;
                if (need_cells != 0U && len >= need_cells * 4U) {
                    node->reg_base = be_cells(value, root_addr_cells);
                    node->reg_size = be_cells(value + root_addr_cells * 4U, root_size_cells);
                    node->have_reg = true;
                }
            } else if (bytes_equal(prop, "interrupts") && len >= 12U) {
                /* GIC binding: type, number, flags. SPI numbers start at 32. */
                u32 type = be32_load(value);
                u32 number = be32_load(value + 4U);
                node->irq = number + (type == 0U ? 32U : 16U);
            } else if (bytes_equal(prop, "bus-range") && len >= 8U) {
                node->bus_start = (u8)be32_load(value);
                node->bus_end = (u8)be32_load(value + 4U);
            } else if (node->is_chosen && bytes_equal(prop, "bootargs")) {
                copy_string(g_platform.bootargs, sizeof(g_platform.bootargs), value, len);
            }
            sp += align4(len);
        } else if (token == FDT_NOP) {
            continue;
        } else if (token == FDT_END) {
            if (depth != 0U) return AEGIS_EINVAL;
            g_platform.valid = true;
            return AEGIS_OK;
        } else {
            return AEGIS_EINVAL;
        }
    }
    return AEGIS_EINVAL;
}

u32 device_tree_get_total_size(void) { return g_platform.valid ? g_platform.total_size : 0U; }
const aegis_dtb_platform_t *device_tree_platform(void) { return &g_platform; }
const aegis_dtb_mmio_device_t *device_tree_virtio(u32 index) {
    if (!g_platform.valid || index >= g_platform.virtio_count) return NULL;
    return &g_platform.virtio[index];
}
bool device_tree_valid(void) { return g_platform.valid; }
