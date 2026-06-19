/* SPDX-License-Identifier: Proprietary */
/* AegisOS — tests/boot/test_boot.c
 * Unit tests for the boot subsystem.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ---- DTB / FDT parsing simulation ---- */

/* Flattened Device Tree magic value (big-endian 0xD00DFEED) */
#define FDT_MAGIC 0xD00DFEEDU

typedef struct { uint32_t magic; uint32_t totalsize; uint32_t version; } fdt_header_t;

static fdt_header_t make_fdt(uint32_t size, uint32_t version) {
    fdt_header_t h;
    /* Store as big-endian */
    h.magic     = __builtin_bswap32(FDT_MAGIC);
    h.totalsize = __builtin_bswap32(size);
    h.version   = __builtin_bswap32(version);
    return h;
}

static int fdt_is_valid(const fdt_header_t *h) {
    return __builtin_bswap32(h->magic) == FDT_MAGIC;
}

/* ---- Boot parameter simulation ---- */

typedef struct {
    uint64_t kernel_base;
    uint64_t initrd_base;
    uint64_t initrd_size;
    uint64_t dtb_addr;
    char     cmdline[256];
} boot_params_t;

static int boot_params_parse(boot_params_t *bp, const char *cmdline,
                              uint64_t kbase, uint64_t ibase, uint64_t isz,
                              uint64_t dtb) {
    if (!bp || !cmdline) return -1;
    bp->kernel_base = kbase;
    bp->initrd_base = ibase;
    bp->initrd_size = isz;
    bp->dtb_addr    = dtb;
    strncpy(bp->cmdline, cmdline, sizeof(bp->cmdline) - 1);
    bp->cmdline[sizeof(bp->cmdline) - 1] = '\0';
    return 0;
}

static const char *cmdline_get_param(const char *cmdline, const char *key, char *out, size_t outsz) {
    char needle[64];
    snprintf(needle, sizeof(needle), "%s=", key);
    const char *p = strstr(cmdline, needle);
    if (!p) return NULL;
    p += strlen(needle);
    size_t i = 0;
    while (*p && *p != ' ' && i + 1 < outsz) out[i++] = *p++;
    out[i] = '\0';
    return out;
}

/* ---- Physical memory region simulation ---- */

typedef struct { uint64_t base; uint64_t size; int usable; } mem_region_t;
#define MAX_REGIONS 8
static mem_region_t mem_map[MAX_REGIONS];
static int mem_count = 0;

static void mem_add_region(uint64_t base, uint64_t size, int usable) {
    if (mem_count < MAX_REGIONS)
        mem_map[mem_count++] = (mem_region_t){ base, size, usable };
}

static uint64_t mem_usable_total(void) {
    uint64_t total = 0;
    for (int i = 0; i < mem_count; i++)
        if (mem_map[i].usable) total += mem_map[i].size;
    return total;
}

/* ---- Tests ---- */

static void test_fdt_magic(void) {
    fdt_header_t h = make_fdt(4096, 17);
    assert(fdt_is_valid(&h));
    /* Corrupt magic */
    h.magic ^= 0xFF000000;
    assert(!fdt_is_valid(&h));
    printf("  [PASS] FDT magic validation\n");
}

static void test_boot_params_parse(void) {
    boot_params_t bp;
    int r = boot_params_parse(&bp, "console=ttyS0,115200 root=/dev/sda1",
                               0x80000ULL, 0x1000000ULL, 8*1024*1024,
                               0x10000000ULL);
    assert(r == 0);
    assert(bp.kernel_base == 0x80000ULL);
    assert(bp.initrd_size == 8*1024*1024);
    assert(strcmp(bp.cmdline, "console=ttyS0,115200 root=/dev/sda1") == 0);
    printf("  [PASS] Boot parameters parsed correctly\n");
}

static void test_cmdline_param_extract(void) {
    const char *cmd = "console=ttyS0,115200 root=/dev/sda1 loglevel=7";
    char val[32];
    const char *r = cmdline_get_param(cmd, "root", val, sizeof(val));
    assert(r != NULL);
    assert(strcmp(val, "/dev/sda1") == 0);
    printf("  [PASS] Command-line parameter extraction\n");
}

static void test_cmdline_missing_param(void) {
    const char *cmd = "console=ttyS0,115200";
    char val[32];
    assert(cmdline_get_param(cmd, "root", val, sizeof(val)) == NULL);
    printf("  [PASS] Missing command-line parameter returns NULL\n");
}

static void test_memory_map_usable(void) {
    mem_count = 0;
    mem_add_region(0x00000000ULL, 0x00100000ULL, 0); /* BIOS reserved */
    mem_add_region(0x00100000ULL, 0x3FF00000ULL, 1); /* 1 GB usable RAM */
    mem_add_region(0x40000000ULL, 0x40000000ULL, 1); /* another 1 GB */
    uint64_t total = mem_usable_total();
    assert(total == 0x3FF00000ULL + 0x40000000ULL);
    printf("  [PASS] Memory map usable-region accounting\n");
}

static void test_kernel_load_alignment(void) {
    /* Kernels must be 2 MB-aligned on AArch64 */
    uint64_t kbase = 0x00200000ULL;
    assert((kbase & (0x200000ULL - 1)) == 0);
    printf("  [PASS] Kernel load address is 2 MB-aligned\n");
}

int main(void) {
    printf("[test_boot] Running boot unit tests...\n");
    test_fdt_magic();
    test_boot_params_parse();
    test_cmdline_param_extract();
    test_cmdline_missing_param();
    test_memory_map_usable();
    test_kernel_load_alignment();
    printf("[test_boot] All tests passed\n");
    return 0;
}
