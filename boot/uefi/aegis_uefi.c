/* SPDX-License-Identifier: Proprietary */
#include "uefi.h"

#define EI_NIDENT 16
#define PT_LOAD 1U
#define MAX_PHDRS 32U
#define QEMU_PL011_BASE 0x09000000ULL

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    UINT16 e_type, e_machine;
    UINT32 e_version;
    UINT64 e_entry, e_phoff, e_shoff;
    UINT32 e_flags;
    UINT16 e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    UINT32 p_type, p_flags;
    UINT64 p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
} Elf64_Phdr;

extern void aegis_uefi_handoff(UINT64 entry, UINT64 dtb, UINT64 load_base, UINT64 load_size);
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *system_table);

/*
 * Keep one absolute image-base relocation in the PE/COFF image.  Without a
 * .reloc directory, EDK2 may only load the application at its preferred
 * ImageBase.  lld-link's default ARM64 ImageBase is outside the 1 GiB QEMU
 * guest RAM window, so firmware rejects BOOTAA64.EFI before efi_main runs.
 */
__attribute__((used))
static void *aegis_pe_relocation_anchor = (void *)&efi_main;

static EFI_SYSTEM_TABLE *st;
static EFI_HANDLE image_handle;
static CHAR16 kernel_path[] = {'\\','E','F','I','\\','A','E','G','I','S','\\','A','E','G','I','S','O','S','.','E','L','F',0};
static CHAR16 msg_load[] = {'A','e','g','i','s','O','S',' ','U','E','F','I',' ','l','o','a','d','e','r','\r','\n',0};
static CHAR16 msg_fail[] = {'A','e','g','i','s','O','S',' ','U','E','F','I',' ','l','o','a','d',' ','f','a','i','l','e','d','\r','\n',0};

static void uart_init(void) {
    volatile UINT32 *uart = (volatile UINT32 *)(uintptr_t)QEMU_PL011_BASE;
    uart[0x30U / 4U] = 0;
    uart[0x24U / 4U] = 13;
    uart[0x28U / 4U] = 21;
    uart[0x2cU / 4U] = 0x70;
    uart[0x30U / 4U] = 0x301;
}

static void uart_putc(char c) {
    volatile UINT32 *uart = (volatile UINT32 *)(uintptr_t)QEMU_PL011_BASE;
    UINT32 guard = 0x200000U;
    while ((uart[0x18U / 4U] & (1U << 5)) != 0U && guard-- != 0U) { }
    uart[0] = (UINT32)(UINT8)c;
}

static void uart_puts(const char *s) {
    while (*s != '\0') {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
}

static void uart_hex64(UINT64 value) {
    static const char digits[] = "0123456789abcdef";
    uart_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        uart_putc(digits[(value >> (UINT32)shift) & 0xfU]);
    }
}

static void fail_stage(const char *stage, EFI_STATUS status) {
    uart_puts("[AegisOS:uefi] failure stage=");
    uart_puts(stage);
    uart_puts(" status=");
    uart_hex64(status);
    uart_puts("\n");
    if (st != 0 && st->ConOut != 0) st->ConOut->OutputString(st->ConOut, msg_fail);
}

static int guid_eq(const EFI_GUID *a, const EFI_GUID *b) {
    const UINT8 *x = (const UINT8 *)a;
    const UINT8 *y = (const UINT8 *)b;
    for (UINTN i = 0; i < sizeof(EFI_GUID); i++) {
        if (x[i] != y[i]) return 0;
    }
    return 1;
}

static void memzero(void *p, UINTN n) {
    UINT8 *b = (UINT8 *)p;
    for (UINTN i = 0; i < n; i++) b[i] = 0;
}

static void print(CHAR16 *s) {
    if (st != 0 && st->ConOut != 0) st->ConOut->OutputString(st->ConOut, s);
}

static EFI_STATUS open_kernel(EFI_FILE_PROTOCOL **file_out, UINTN *size_out) {
    EFI_BOOT_SERVICES *bs = st->BootServices;
    EFI_LOADED_IMAGE_PROTOCOL *loaded = 0;
    EFI_STATUS rc = bs->HandleProtocol(image_handle,
                                       (EFI_GUID *)&EFI_LOADED_IMAGE_PROTOCOL_GUID,
                                       (void **)&loaded);
    if (EFI_ERROR(rc) || loaded == 0) return EFI_ERROR(rc) ? rc : EFI_NOT_FOUND;

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = 0;
    rc = bs->HandleProtocol(loaded->DeviceHandle,
                            (EFI_GUID *)&EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID,
                            (void **)&fs);
    if (EFI_ERROR(rc) || fs == 0) return EFI_ERROR(rc) ? rc : EFI_NOT_FOUND;

    EFI_FILE_PROTOCOL *root = 0;
    EFI_FILE_PROTOCOL *file = 0;
    rc = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(rc) || root == 0) return EFI_ERROR(rc) ? rc : EFI_NOT_FOUND;

    rc = root->Open(root, &file, kernel_path, EFI_FILE_MODE_READ, 0);
    root->Close(root);
    if (EFI_ERROR(rc) || file == 0) return EFI_ERROR(rc) ? rc : EFI_NOT_FOUND;

    UINT8 info_storage[512];
    UINTN info_size = sizeof(info_storage);
    rc = file->GetInfo(file, (EFI_GUID *)&EFI_FILE_INFO_GUID, &info_size, info_storage);
    if (EFI_ERROR(rc)) {
        file->Close(file);
        return rc;
    }

    EFI_FILE_INFO *info = (EFI_FILE_INFO *)info_storage;
    if (info->FileSize < sizeof(Elf64_Ehdr) || info->FileSize > (64ULL * 1024ULL * 1024ULL)) {
        file->Close(file);
        return EFI_LOAD_ERROR;
    }

    *file_out = file;
    *size_out = (UINTN)info->FileSize;
    return EFI_SUCCESS;
}

static EFI_STATUS read_exact(EFI_FILE_PROTOCOL *file, UINT64 offset, void *buffer, UINTN size) {
    EFI_STATUS rc = file->SetPosition(file, offset);
    if (EFI_ERROR(rc)) return rc;
    UINTN got = size;
    rc = file->Read(file, &got, buffer);
    if (EFI_ERROR(rc)) return rc;
    return got == size ? EFI_SUCCESS : EFI_LOAD_ERROR;
}

static EFI_STATUS load_kernel_elf(EFI_FILE_PROTOCOL *file,
                                  UINTN file_size,
                                  UINT64 *entry_out,
                                  UINT64 *load_base_out,
                                  UINT64 *load_size_out) {
    Elf64_Ehdr eh;
    Elf64_Phdr ph[MAX_PHDRS];
    EFI_STATUS rc = read_exact(file, 0, &eh, sizeof(eh));
    if (EFI_ERROR(rc)) return rc;

    if (eh.e_ident[0] != 0x7f || eh.e_ident[1] != 'E' ||
        eh.e_ident[2] != 'L' || eh.e_ident[3] != 'F' ||
        eh.e_ident[4] != 2 || eh.e_ident[5] != 1 || eh.e_machine != 183) {
        return EFI_LOAD_ERROR;
    }
    if (eh.e_phentsize != sizeof(Elf64_Phdr) || eh.e_phnum == 0 || eh.e_phnum > MAX_PHDRS) {
        return EFI_LOAD_ERROR;
    }

    UINT64 ph_bytes = (UINT64)eh.e_phnum * sizeof(Elf64_Phdr);
    if (eh.e_phoff > file_size || ph_bytes > file_size - eh.e_phoff) return EFI_LOAD_ERROR;
    rc = read_exact(file, eh.e_phoff, ph, (UINTN)ph_bytes);
    if (EFI_ERROR(rc)) return rc;

    UINT64 min_page = ~0ULL;
    UINT64 max_page = 0;
    for (UINT16 i = 0; i < eh.e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD || ph[i].p_memsz == 0) continue;
        if (ph[i].p_filesz > ph[i].p_memsz || ph[i].p_offset > file_size ||
            ph[i].p_filesz > file_size - ph[i].p_offset) {
            return EFI_LOAD_ERROR;
        }
        UINT64 base = ph[i].p_paddr != 0 ? ph[i].p_paddr : ph[i].p_vaddr;
        if (base + ph[i].p_memsz < base) return EFI_LOAD_ERROR;
        UINT64 first = base & ~(EFI_PAGE_SIZE - 1ULL);
        UINT64 last = (base + ph[i].p_memsz + EFI_PAGE_SIZE - 1ULL) & ~(EFI_PAGE_SIZE - 1ULL);
        if (first < min_page) min_page = first;
        if (last > max_page) max_page = last;
    }
    if (min_page == ~0ULL || max_page <= min_page || eh.e_entry < min_page || eh.e_entry >= max_page) {
        return EFI_LOAD_ERROR;
    }

    EFI_PHYSICAL_ADDRESS region = min_page;
    UINTN pages = (UINTN)((max_page - min_page) / EFI_PAGE_SIZE);
    rc = st->BootServices->AllocatePages(EFI_ALLOCATE_ADDRESS, EFI_LOADER_DATA, pages, &region);
    if (EFI_ERROR(rc) || region != min_page) return EFI_ERROR(rc) ? rc : EFI_LOAD_ERROR;

    memzero((void *)(uintptr_t)min_page, (UINTN)(max_page - min_page));
    uart_puts("[AegisOS:uefi] kernel load region reserved base=");
    uart_hex64(min_page);
    uart_puts(" size=");
    uart_hex64(max_page - min_page);
    uart_puts("\n");

    for (UINT16 i = 0; i < eh.e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD || ph[i].p_memsz == 0 || ph[i].p_filesz == 0) continue;
        UINT64 base = ph[i].p_paddr != 0 ? ph[i].p_paddr : ph[i].p_vaddr;
        rc = read_exact(file, ph[i].p_offset, (void *)(uintptr_t)base, (UINTN)ph[i].p_filesz);
        if (EFI_ERROR(rc)) return rc;
    }

    *entry_out = eh.e_entry;
    *load_base_out = min_page;
    *load_size_out = max_page - min_page;
    return EFI_SUCCESS;
}

static void *find_dtb(void) {
    for (UINTN i = 0; i < st->NumberOfTableEntries; i++) {
        if (guid_eq(&st->ConfigurationTable[i].VendorGuid, &EFI_DTB_TABLE_GUID)) {
            return st->ConfigurationTable[i].VendorTable;
        }
    }
    return 0;
}

static EFI_STATUS exit_boot_services(void) {
    EFI_BOOT_SERVICES *bs = st->BootServices;
    for (int attempt = 0; attempt < 8; attempt++) {
        UINTN map_size = 0;
        UINTN map_key = 0;
        UINTN desc_size = 0;
        UINT32 desc_ver = 0;
        void *map = 0;
        EFI_STATUS rc = bs->GetMemoryMap(&map_size, 0, &map_key, &desc_size, &desc_ver);
        if (rc != EFI_BUFFER_TOO_SMALL || desc_size == 0) return rc;
        map_size += desc_size * 16U;
        rc = bs->AllocatePool(EFI_LOADER_DATA, map_size, &map);
        if (EFI_ERROR(rc)) return rc;
        rc = bs->GetMemoryMap(&map_size, map, &map_key, &desc_size, &desc_ver);
        if (!EFI_ERROR(rc)) rc = bs->ExitBootServices(image_handle, map_key);
        if (!EFI_ERROR(rc)) return EFI_SUCCESS;
        bs->FreePool(map);
    }
    return EFI_LOAD_ERROR;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *system_table) {
    image_handle = image;
    st = system_table;
    uart_init();
    uart_puts("[AegisOS:uefi] loader entered\n");
    print(msg_load);

    EFI_FILE_PROTOCOL *file = 0;
    UINTN file_size = 0;
    EFI_STATUS rc = open_kernel(&file, &file_size);
    if (EFI_ERROR(rc)) {
        fail_stage("open-kernel", rc);
        return rc;
    }
    uart_puts("[AegisOS:uefi] kernel file opened\n");

    UINT64 entry = 0;
    UINT64 load_base = 0;
    UINT64 load_size = 0;
    rc = load_kernel_elf(file, file_size, &entry, &load_base, &load_size);
    file->Close(file);
    if (EFI_ERROR(rc)) {
        fail_stage("load-elf", rc);
        return rc;
    }
    uart_puts("[AegisOS:uefi] ELF segments loaded entry=");
    uart_hex64(entry);
    uart_puts("\n");

    void *dtb = find_dtb();
    if (dtb == 0) {
        fail_stage("find-dtb", EFI_NOT_FOUND);
        return EFI_NOT_FOUND;
    }
    uart_puts("[AegisOS:uefi] FDT located at ");
    uart_hex64((UINT64)(uintptr_t)dtb);
    uart_puts("\n");

    rc = exit_boot_services();
    if (EFI_ERROR(rc)) {
        fail_stage("exit-boot-services", rc);
        return rc;
    }

    uart_puts("[AegisOS:uefi] ExitBootServices complete\n");
    uart_puts("[AegisOS:uefi] handing off to EL1\n");
    aegis_uefi_handoff(entry, (UINT64)(uintptr_t)dtb, load_base, load_size);
    for (;;) __asm__ volatile("wfe");
}
