/* SPDX-License-Identifier: Proprietary */
/* AegisOS — fs/initramfs.c
 * Initial RAM filesystem — provides an in-memory read-only filesystem
 * loaded from the CPIO archive embedded in the kernel image.
 * Used during early boot before persistent storage is available.
 */

#include "include/initramfs.h"
#include "include/vfs.h"
#include "../kernel/include/memory.h"
#include "../kernel/include/panic.h"
#include "../kernel/include/userland.h"

#define INITRAMFS_MAX_FILES  256
#define INITRAMFS_MAX_NAME   128

/* CPIO newc header (SVR4 portable format) */
typedef struct {
    char magic[6];      /* "070701" */
    char ino[8];
    char mode[8];
    char uid[8];
    char gid[8];
    char nlink[8];
    char mtime[8];
    char filesize[8];
    char devmajor[8];
    char devminor[8];
    char rdevmajor[8];
    char rdevminor[8];
    char namesize[8];
    char check[8];
} cpio_header_t;

typedef struct {
    char name[INITRAMFS_MAX_NAME];
    const u8 *data;
    u64 size;
    u32 mode;
    bool is_dir;
    bool valid;
} initramfs_entry_t;

static initramfs_entry_t files[INITRAMFS_MAX_FILES];
static u32 file_count = 0;
static bool mounted = false;


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
#define PF_R 4
#define AEGIS_ELF_EM_AARCH64 183u

typedef struct initramfs_elf64_ehdr {
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
} initramfs_elf64_ehdr_t;

typedef struct initramfs_elf64_phdr {
    u32 p_type;
    u32 p_flags;
    u64 p_offset;
    u64 p_vaddr;
    u64 p_paddr;
    u64 p_filesz;
    u64 p_memsz;
    u64 p_align;
} initramfs_elf64_phdr_t;

typedef struct initramfs_builtin_aegis_init_elf {
    initramfs_elf64_ehdr_t ehdr;
    initramfs_elf64_phdr_t text;
    u8 payload[16];
} initramfs_builtin_aegis_init_elf_t;

static const initramfs_builtin_aegis_init_elf_t initramfs_aegis_init_elf = {
    .ehdr = {
        .e_ident = {0x7f, 'E', 'L', 'F', ELFCLASS64, ELFDATA2LSB, EV_CURRENT, 0},
        .e_type = ET_EXEC,
        .e_machine = AEGIS_ELF_EM_AARCH64,
        .e_version = EV_CURRENT,
        .e_entry = AEGIS_USER_TEXT_BASE,
        .e_phoff = sizeof(initramfs_elf64_ehdr_t),
        .e_shoff = 0,
        .e_flags = 0,
        .e_ehsize = sizeof(initramfs_elf64_ehdr_t),
        .e_phentsize = sizeof(initramfs_elf64_phdr_t),
        .e_phnum = 1,
        .e_shentsize = 0,
        .e_shnum = 0,
        .e_shstrndx = 0,
    },
    .text = {
        .p_type = PT_LOAD,
        .p_flags = PF_R | PF_X,
        .p_offset = sizeof(initramfs_elf64_ehdr_t) + sizeof(initramfs_elf64_phdr_t),
        .p_vaddr = AEGIS_USER_TEXT_BASE,
        .p_paddr = AEGIS_USER_TEXT_BASE,
        .p_filesz = sizeof(((initramfs_builtin_aegis_init_elf_t *)0)->payload),
        .p_memsz = PAGE_SIZE,
        .p_align = PAGE_SIZE,
    },
    .payload = {0xa9, 0x36, 0x00, 0x00, 0x54, 0x36, 0x00, 0x00,
                0x45, 0x4c, 0x46, 0x00, 0x56, 0x46, 0x53, 0x00},
};

static void initramfs_copy_name(char *dst, const char *src) {
    u32 i = 0;
    while (src && src[i] && i < INITRAMFS_MAX_NAME - 1U) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int initramfs_add_entry(const char *name, const void *data, u64 size, u32 mode, bool is_dir) {
    if (!name || file_count >= INITRAMFS_MAX_FILES) return AEGIS_EINVAL;
    initramfs_entry_t *entry = &files[file_count++];
    initramfs_copy_name(entry->name, name);
    entry->data = (const u8 *)data;
    entry->size = size;
    entry->mode = mode;
    entry->is_dir = is_dir;
    entry->valid = true;
    return AEGIS_OK;
}

static u32 hex8_to_u32(const char *s) {
    u32 val = 0;
    for (int i = 0; i < 8; i++) {
        val <<= 4;
        char c = s[i];
        if (c >= '0' && c <= '9') val |= (u32)(c - '0');
        else if (c >= 'a' && c <= 'f') val |= (u32)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') val |= (u32)(c - 'A' + 10);
    }
    return val;
}

static u64 align4(u64 offset) {
    return (offset + 3) & ~3ULL;
}

/* VFS operations for initramfs */
static initramfs_entry_t *initramfs_lookup_internal(const char *path);

static int initramfs_read(vnode_t *vn, u64 offset, void *buf, u64 len) {
    if (!vn || !buf) return AEGIS_EINVAL;
    const char *path = (const char *)vn->priv;
    initramfs_entry_t *entry = initramfs_lookup_internal(path);
    if (!entry) return AEGIS_ENOENT;
    if (offset >= entry->size) return 0;
    u64 avail = entry->size - offset;
    u64 to_read = len < avail ? len : avail;
    const u8 *src = entry->data + offset;
    u8 *dst = (u8 *)buf;
    for (u64 i = 0; i < to_read; i++) dst[i] = src[i];
    return (int)to_read;
}

static int initramfs_readdir(vnode_t *vn, u32 index, char *name, vnode_t **out) {
    (void)vn; (void)out;
    if (index >= file_count) return -1;
    if (!files[index].valid) return -1;
    /* Copy filename */
    const char *src = files[index].name;
    int i = 0;
    while (src[i] && i < VFS_NAME_MAX - 1) { name[i] = src[i]; i++; }
    name[i] = '\0';
    return 0;
}

static vnode_ops_t initramfs_ops = {
    .read = initramfs_read,
    .write = NULL,
    .readdir = initramfs_readdir,
    .stat = NULL,
};

static vfs_t initramfs_vfs = {
    .name = "initramfs",
    .mount = NULL,
    .ops = &initramfs_ops,
    .priv = NULL,
};

int initramfs_mount(const void *data, u64 size) {
    if (!data || size == 0) return AEGIS_EINVAL;
    if (mounted) return AEGIS_EBUSY;

    const u8 *ptr = (const u8 *)data;
    const u8 *end = ptr + size;
    file_count = 0;

    while (ptr + sizeof(cpio_header_t) <= end && file_count < INITRAMFS_MAX_FILES) {
        const cpio_header_t *hdr = (const cpio_header_t *)ptr;

        /* Verify magic */
        if (hdr->magic[0] != '0' || hdr->magic[1] != '7' ||
            hdr->magic[2] != '0' || hdr->magic[3] != '7' ||
            hdr->magic[4] != '0' || hdr->magic[5] != '1') {
            break;
        }

        u32 namesize = hex8_to_u32(hdr->namesize);
        u32 filesize = hex8_to_u32(hdr->filesize);
        u32 mode = hex8_to_u32(hdr->mode);

        const char *name = (const char *)(ptr + sizeof(cpio_header_t));

        /* Check for TRAILER marker */
        if (namesize == 11) {
            bool is_trailer = true;
            const char *trailer = "TRAILER!!!";
            for (int i = 0; i < 10; i++) {
                if (name[i] != trailer[i]) { is_trailer = false; break; }
            }
            if (is_trailer) break;
        }

        /* Skip "." entry */
        if (namesize > 1 || name[0] != '.') {
            initramfs_entry_t *entry = &files[file_count];
            /* Copy name (skip leading ./) */
            const char *n = name;
            if (n[0] == '.' && n[1] == '/') n += 2;
            int i = 0;
            while (*n && i < INITRAMFS_MAX_NAME - 1) { entry->name[i++] = *n++; }
            entry->name[i] = '\0';

            u64 data_offset = align4(sizeof(cpio_header_t) + namesize);
            entry->data = ptr + data_offset;
            entry->size = filesize;
            entry->mode = mode;
            entry->is_dir = (mode & 0040000) != 0;
            entry->valid = true;
            file_count++;
        }

        /* Advance to next entry */
        u64 header_plus_name = align4(sizeof(cpio_header_t) + namesize);
        u64 total = header_plus_name + align4(filesize);
        ptr += total;
    }

    /* Mount at / */
    vfs_mount("/", &initramfs_vfs);
    mounted = true;
    return AEGIS_OK;
}

int initramfs_mount_empty_root(void) {
    if (mounted) return AEGIS_EBUSY;
    file_count = 0;
    for (u32 i = 0; i < INITRAMFS_MAX_FILES; i++) {
        files[i].valid = false;
        files[i].name[0] = '\0';
        files[i].data = NULL;
        files[i].size = 0;
        files[i].mode = 0;
        files[i].is_dir = false;
    }
    int rc = vfs_mount("/", &initramfs_vfs);
    if (rc != AEGIS_OK) return rc;
    mounted = true;
    return AEGIS_OK;
}

bool initramfs_is_mounted(void) {
    return mounted;
}

/* Lookup a file by path within initramfs */
static initramfs_entry_t *initramfs_lookup_internal(const char *path) {
    if (!path) return NULL;
    /* Skip leading / */
    if (*path == '/') path++;

    for (u32 i = 0; i < file_count; i++) {
        if (!files[i].valid) continue;
        const char *a = path;
        const char *b = files[i].name;
        bool match = true;
        while (*a && *b) {
            if (*a != *b) { match = false; break; }
            a++; b++;
        }
        if (match && *a == '\0' && *b == '\0') {
            return &files[i];
        }
    }
    return NULL;
}

initramfs_entry_t *initramfs_lookup(const char *path) {
    return initramfs_lookup_internal(path);
}



int initramfs_install_builtin_userland(void) {
    if (!mounted) return AEGIS_EINVAL;
    if (initramfs_lookup_internal("/sbin/aegis-init")) return AEGIS_EBUSY;
    int rc = initramfs_add_entry("sbin", NULL, 0, 0040755, true);
    if (rc != AEGIS_OK) return rc;
    return initramfs_add_entry("sbin/aegis-init",
                               &initramfs_aegis_init_elf,
                               sizeof(initramfs_aegis_init_elf),
                               0100755,
                               false);
}

bool initramfs_has_file(const char *path) {
    initramfs_entry_t *entry = initramfs_lookup_internal(path);
    return entry && entry->valid && !entry->is_dir;
}

u64 initramfs_file_size(const char *path) {
    initramfs_entry_t *entry = initramfs_lookup_internal(path);
    if (!entry || !entry->valid || entry->is_dir) return 0;
    return entry->size;
}


static const char rootfs_aegisos_toml[] =
    "[aegisos]\n"
    "version = \"v2.0-v55.1-interactive-runtime\"\n"
    "edition = \"release\"\n"
    "console = \"ttyAMA0\"\n"
    "shell = \"/bin/aegis-shell\"\n"
    "rootfs_mode = \"initramfs-bridged\"\n";

static const char rootfs_services_toml[] =
    "[services]\n"
    "aegis-init = \"/userland/init\"\n"
    "aegis-shell = \"/userland/shell\"\n"
    "aegisctl = \"/userland/aegisctl\"\n"
    "netctl = \"/userland/netctl\"\n"
    "service-manager = \"/userland/service-manager\"\n"
    "diagnostics = \"/userland/diagnostics\"\n"
    "recovery = \"/userland/recovery\"\n"
    "update-manager = \"/userland/update-manager\"\n"
    "rustmyadmin = \"/rustmyadmin\"\n"
    "dashboard = \"/dashboard\"\n"
    "dns-filter = \"/services/dns-filter\"\n"
    "firewall = \"/services/firewall\"\n"
    "vpn = \"/services/vpn\"\n"
    "traffic-monitor = \"/services/traffic-monitor\"\n"
    "intrusion-watch = \"/services/intrusion-watch\"\n"
    "cloudflare-tunnel = \"/services/cloudflare-tunnel\"\n"
    "container-runner = \"/services/container-runner\"\n"
    "device-discovery = \"/services/device-discovery\"\n"
    "radio-manager = \"/services/radio-manager\"\n"
    "web-hosting = \"/services/web-hosting\"\n";

static const char rootfs_network_toml[] =
    "[network]\n"
    "profile = \"aegisbox-router-dp\"\n"
    "wan = \"eth0\"\n"
    "lan = \"eth1\"\n"
    "dhcp = true\n"
    "nat = true\n"
    "firewall = true\n";

static const char rootfs_firewall_toml[] =
    "[firewall]\n"
    "default_input = \"drop\"\n"
    "default_forward = \"drop\"\n"
    "allow_established = true\n"
    "allow_lan_admin = true\n"
    "nat_masquerade_wan = true\n";

static const char rootfs_hardening_toml[] =
    "[hardening]\n"
    "user_kernel_page_table_isolation = true\n"
    "stack_guard_pages = true\n"
    "service_supervisor_fault_accounting = true\n"
    "rustmyadmin_least_privilege = true\n"
    "release_checksums_required = true\n";

static const char rootfs_rust_bridge_manifest[] =
    "AegisOS v55.1 Rust bridge manifest\n"
    "userland/init -> /sbin/aegis-init\n"
    "userland/shell -> /bin/aegis-shell\n"
    "userland/aegisctl -> /bin/aegisctl\n"
    "userland/netctl -> /bin/netctl\n"
    "userland/service-manager -> /sbin/aegis-service-manager\n"
    "rustmyadmin -> /opt/rustmyadmin\n"
    "dashboard -> /opt/aegisos-dashboard\n"
    "services/* -> supervised service catalogue\n"
    "security/* -> hardening/signing/policy catalogue\n"
    "NOTE: v55.1 connects/catalogues the Rust source trees to the shell. Native Rust execution comes after the AegisOS Rust target and userspace ABI are promoted.\n";

static const char rootfs_rustmyadmin_manifest[] =
    "RustMyAdmin integration\n"
    "source=/rustmyadmin\n"
    "rootfs=/opt/rustmyadmin\n"
    "routes=auth,backups,containers,dashboard,firewall,logs,router,services,system,updates,users,vpn\n"
    "security=csrf,passwords,roles,sessions,audit,headers,permissions,rate-limit\n"
    "status=connected-to-aegis-shell-catalogue; network web runtime awaits sockets/TCP + Rust userspace ABI\n";

static const char rootfs_shell_stub[] =
    "#!/aegisos/kernel-console\n"
    "Aegis shell is provided by the v55.1 kernel-hosted serial console bridge.\n"
    "Commands: help, status, services, rust, rustmyadmin, net, variants, rootfs, ls, cat, run.\n";

static int initramfs_add_entry_if_missing(const char *name, const void *data, u64 size, u32 mode, bool is_dir) {
    if (initramfs_lookup_internal(name)) return AEGIS_OK;
    return initramfs_add_entry(name, data, size, mode, is_dir);
}

int initramfs_install_release_rootfs(void) {
    if (!mounted) return AEGIS_EINVAL;
    int rc = AEGIS_OK;
#define ADD_DIR(path) do { rc = initramfs_add_entry_if_missing((path), NULL, 0, 0040755, true); if (rc != AEGIS_OK) return rc; } while (0)
#define ADD_FILE(path, data) do { rc = initramfs_add_entry_if_missing((path), (data), sizeof(data) - 1U, 0100644, false); if (rc != AEGIS_OK) return rc; } while (0)
#define ADD_EXEC(path, data) do { rc = initramfs_add_entry_if_missing((path), (data), sizeof(data) - 1U, 0100755, false); if (rc != AEGIS_OK) return rc; } while (0)
    ADD_DIR("bin");
    ADD_DIR("sbin");
    ADD_DIR("etc");
    ADD_DIR("etc/aegisos");
    ADD_DIR("opt");
    ADD_DIR("opt/rustmyadmin");
    ADD_DIR("services");
    ADD_DIR("userland");
    ADD_DIR("security");
    ADD_DIR("var");
    ADD_DIR("var/log");
    ADD_DIR("var/lib");
    ADD_DIR("var/backups");
    ADD_EXEC("bin/aegis-shell", rootfs_shell_stub);
    ADD_EXEC("bin/aegisctl", rootfs_shell_stub);
    ADD_EXEC("bin/netctl", rootfs_shell_stub);
    ADD_FILE("etc/aegisos.toml", rootfs_aegisos_toml);
    ADD_FILE("etc/services.toml", rootfs_services_toml);
    ADD_FILE("etc/network.toml", rootfs_network_toml);
    ADD_FILE("etc/firewall.toml", rootfs_firewall_toml);
    ADD_FILE("etc/hardening.toml", rootfs_hardening_toml);
    ADD_FILE("etc/aegisos/rust-bridge.manifest", rootfs_rust_bridge_manifest);
    ADD_FILE("opt/rustmyadmin/INTEGRATION.txt", rootfs_rustmyadmin_manifest);
#undef ADD_EXEC
#undef ADD_FILE
#undef ADD_DIR
    return AEGIS_OK;
}

const char *initramfs_file_name(u32 index) {
    if (index >= file_count || !files[index].valid) return NULL;
    return files[index].name;
}

bool initramfs_file_is_dir(u32 index) {
    if (index >= file_count || !files[index].valid) return false;
    return files[index].is_dir;
}

int initramfs_read_text_file(const char *path, const char **out_data, u64 *out_size) {
    if (!path || !out_data || !out_size) return AEGIS_EINVAL;
    initramfs_entry_t *entry = initramfs_lookup_internal(path);
    if (!entry || !entry->valid || entry->is_dir) return AEGIS_ENOENT;
    *out_data = (const char *)entry->data;
    *out_size = entry->size;
    return AEGIS_OK;
}

u32 initramfs_file_count(void) {
    return file_count;
}
