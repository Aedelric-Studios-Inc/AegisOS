/* SPDX-License-Identifier: Proprietary */
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef uint64_t EFI_STATUS;
typedef void *EFI_HANDLE;
typedef uint16_t CHAR16;
typedef uint64_t EFI_PHYSICAL_ADDRESS;
typedef uint64_t EFI_VIRTUAL_ADDRESS;
typedef uint64_t UINTN;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint8_t BOOLEAN;

#define EFIAPI
#define EFI_SUCCESS 0
#define EFI_ERROR(x) (((x) & (1ULL << 63)) != 0)
#define EFI_BUFFER_TOO_SMALL (1ULL << 63 | 5)
#define EFI_NOT_FOUND (1ULL << 63 | 14)
#define EFI_LOAD_ERROR (1ULL << 63 | 1)
#define EFI_INVALID_PARAMETER (1ULL << 63 | 2)
#define EFI_OUT_OF_RESOURCES (1ULL << 63 | 9)
#define EFI_FILE_MODE_READ 1ULL
#define EFI_ALLOCATE_ADDRESS 2U
#define EFI_LOADER_DATA 2U
#define EFI_PAGE_SIZE 4096ULL
#define EFI_SIZE_TO_PAGES(x) (((x) + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE)

typedef struct { UINT32 Data1; UINT16 Data2; UINT16 Data3; UINT8 Data4[8]; } EFI_GUID;
typedef struct { EFI_GUID VendorGuid; void *VendorTable; } EFI_CONFIGURATION_TABLE;

typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    void *Reset;
    EFI_STATUS (EFIAPI *OutputString)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *, CHAR16 *);
};

typedef struct EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;
struct EFI_FILE_PROTOCOL {
    UINT64 Revision;
    EFI_STATUS (EFIAPI *Open)(EFI_FILE_PROTOCOL *, EFI_FILE_PROTOCOL **, CHAR16 *, UINT64, UINT64);
    EFI_STATUS (EFIAPI *Close)(EFI_FILE_PROTOCOL *);
    void *Delete;
    EFI_STATUS (EFIAPI *Read)(EFI_FILE_PROTOCOL *, UINTN *, void *);
    void *Write;
    EFI_STATUS (EFIAPI *GetPosition)(EFI_FILE_PROTOCOL *, UINT64 *);
    EFI_STATUS (EFIAPI *SetPosition)(EFI_FILE_PROTOCOL *, UINT64);
    EFI_STATUS (EFIAPI *GetInfo)(EFI_FILE_PROTOCOL *, EFI_GUID *, UINTN *, void *);
};

typedef struct {
    UINT64 Revision;
    EFI_STATUS (EFIAPI *OpenVolume)(void *, EFI_FILE_PROTOCOL **);
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

typedef struct {
    UINT32 Revision;
    EFI_HANDLE ParentHandle;
    void *SystemTable;
    EFI_HANDLE DeviceHandle;
    void *FilePath;
    void *Reserved;
    UINT32 LoadOptionsSize;
    void *LoadOptions;
    void *ImageBase;
    UINT64 ImageSize;
    UINT32 ImageCodeType;
    UINT32 ImageDataType;
    EFI_STATUS (EFIAPI *Unload)(EFI_HANDLE);
} EFI_LOADED_IMAGE_PROTOCOL;

typedef struct EFI_BOOT_SERVICES EFI_BOOT_SERVICES;
struct EFI_BOOT_SERVICES {
    char _pad0[24];
    void *RaiseTPL; void *RestoreTPL;
    EFI_STATUS (EFIAPI *AllocatePages)(UINT32, UINT32, UINTN, EFI_PHYSICAL_ADDRESS *);
    EFI_STATUS (EFIAPI *FreePages)(EFI_PHYSICAL_ADDRESS, UINTN);
    EFI_STATUS (EFIAPI *GetMemoryMap)(UINTN *, void *, UINTN *, UINTN *, UINT32 *);
    EFI_STATUS (EFIAPI *AllocatePool)(UINT32, UINTN, void **);
    EFI_STATUS (EFIAPI *FreePool)(void *);
    void *CreateEvent;
    void *SetTimer;
    void *WaitForEvent;
    void *SignalEvent;
    void *CloseEvent;
    void *CheckEvent;
    void *InstallProtocolInterface;
    void *ReinstallProtocolInterface;
    void *UninstallProtocolInterface;
    EFI_STATUS (EFIAPI *HandleProtocol)(EFI_HANDLE, EFI_GUID *, void **);
    void *Reserved;
    void *RegisterProtocolNotify;
    void *LocateHandle;
    void *LocateDevicePath;
    void *InstallConfigurationTable;
    void *LoadImage;
    void *StartImage;
    void *Exit;
    void *UnloadImage;
    EFI_STATUS (EFIAPI *ExitBootServices)(EFI_HANDLE, UINTN);
};

typedef struct {
    char _hdr[24];
    CHAR16 *FirmwareVendor;
    UINT32 FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    void *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    void *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    UINTN NumberOfTableEntries;
    EFI_CONFIGURATION_TABLE *ConfigurationTable;
} EFI_SYSTEM_TABLE;

typedef struct {
    UINT64 Size;
    UINT64 FileSize;
    UINT64 PhysicalSize;
    UINT64 CreateTime[2];
    UINT64 LastAccessTime[2];
    UINT64 ModificationTime[2];
    UINT64 Attribute;
    CHAR16 FileName[1];
} EFI_FILE_INFO;

static const EFI_GUID EFI_LOADED_IMAGE_PROTOCOL_GUID = {0x5B1B31A1,0x9562,0x11d2,{0x8E,0x3F,0x00,0xA0,0xC9,0x69,0x72,0x3B}};
static const EFI_GUID EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID = {0x964E5B22,0x6459,0x11d2,{0x8E,0x39,0x00,0xA0,0xC9,0x69,0x72,0x3B}};
static const EFI_GUID EFI_FILE_INFO_GUID = {0x09576E92,0x6D3F,0x11D2,{0x8E,0x39,0x00,0xA0,0xC9,0x69,0x72,0x3B}};
static const EFI_GUID EFI_DTB_TABLE_GUID = {0xB1B621D5,0xF19C,0x41A5,{0x83,0x0B,0xD9,0x15,0x2C,0x69,0xAA,0xE0}};
