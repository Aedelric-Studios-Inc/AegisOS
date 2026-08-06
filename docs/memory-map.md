# AegisOS Memory Map

## Physical Address Space (AArch64)

| Region              | Start Address  | Size    | Description                        |
|---------------------|----------------|---------|------------------------------------|
| Boot ROM            | 0x0000_0000    | 64 KB   | On-chip ROM (vendor)               |
| Device peripherals  | 0xFE00_0000    | 16 MB   | MMIO for SoC peripherals           |
| DRAM                | 0x4000_0000    | varies  | Main system RAM                    |
| Kernel image        | 0x4008_0000    | 32 MB   | Loaded kernel text + data          |
| Kernel heap         | 0x4208_0000    | 64 MB   | Dynamic kernel allocations         |
| Userland start      | 0x0000_4000_0000_0000 | — | AArch64 user virtual address space |

## Virtual Address Space

| Region              | Virtual Address              | Description              |
|---------------------|------------------------------|--------------------------|
| Kernel image        | 0xFFFF_FF80_0000_0000        | Kernel text + data       |
| Kernel heap         | 0xFFFF_FF88_0000_0000        | Kernel heap              |
| MMIO (HAL)          | 0xFFFF_FFC0_0000_0000        | Mapped device registers  |
| User space          | 0x0000_0000_0000_0000        | Process virtual memory   |

## Page Size

AegisOS uses 4 KB pages with a 4-level translation table (TTB0 for user, TTB1 for kernel).
