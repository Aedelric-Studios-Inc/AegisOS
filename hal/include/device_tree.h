/* SPDX-License-Identifier: Proprietary */
#pragma once
/* AegisOS flattened-device-tree discovery interface. */

#include "../../kernel/include/types.h"

#define AEGIS_DTB_BOOTARGS_MAX 256U
#define AEGIS_DTB_VIRTIO_MAX   16U

typedef struct aegis_dtb_mmio_device {
    u64 base;
    u64 size;
    u32 irq;
    u32 device_id;
    bool present;
} aegis_dtb_mmio_device_t;

typedef struct aegis_dtb_platform {
    bool valid;
    u32 total_size;
    u64 memory_base;
    u64 memory_size;
    u64 uart_base;
    u64 gic_dist_base;
    u64 gic_cpu_base;
    u64 rtc_base;
    u64 pci_ecam_base;
    u64 pci_ecam_size;
    u8 pci_bus_start;
    u8 pci_bus_end;
    char bootargs[AEGIS_DTB_BOOTARGS_MAX];
    aegis_dtb_mmio_device_t virtio[AEGIS_DTB_VIRTIO_MAX];
    u32 virtio_count;
} aegis_dtb_platform_t;

int  device_tree_init(const void *fdt);
u32  device_tree_get_total_size(void);
const aegis_dtb_platform_t *device_tree_platform(void);
const aegis_dtb_mmio_device_t *device_tree_virtio(u32 index);
bool device_tree_valid(void);
