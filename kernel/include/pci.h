/* SPDX-License-Identifier: Proprietary */
#pragma once
#include "types.h"

typedef struct aegis_pci_device {
    u8 bus, device, function;
    u16 vendor_id, device_id;
    u8 class_code, subclass, prog_if;
    u64 bar[6];
    u32 irq;
} aegis_pci_device_t;

void pci_init(u64 ecam_base, u8 bus_start, u8 bus_end);
int  pci_scan(void);
u32  pci_device_count(void);
const aegis_pci_device_t *pci_device(u32 index);
const aegis_pci_device_t *pci_find_class(u8 class_code, u8 subclass, u8 prog_if, u32 ordinal);
int  pci_enable_memory_busmaster(const aegis_pci_device_t *dev);
