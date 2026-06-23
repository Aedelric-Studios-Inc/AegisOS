/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/core/interactive_console.c
 * v55.1 interactive runtime console and source/service bridge.
 */

#include "interactive_console.h"
#include "userland.h"
#include "service_manager.h"
#include "service_supervisor.h"
#include "release_final.h"
#include "network_control.h"
#include "vfs.h"
#include "initramfs.h"
#include "types.h"

extern void uart_putchar(char c);
extern int  uart_getchar(void);
extern void scheduler_yield(void);

#define AEGIS_CONSOLE_LINE_MAX 128U

typedef struct aegis_console_runtime_asset {
    const char *name;
    const char *kind;
    const char *path;
    const char *status;
} aegis_console_runtime_asset_t;

static const aegis_console_runtime_asset_t runtime_assets[] = {
    {"aegis-init",        "rust-userland", "/userland/init",              "catalogued + rootfs-linked"},
    {"aegis-shell",       "rust-userland", "/userland/shell",             "catalogued + interactive console bridge active"},
    {"aegisctl",          "rust-userland", "/userland/aegisctl",          "catalogued"},
    {"netctl",            "rust-userland", "/userland/netctl",            "catalogued + v46 network state bridged"},
    {"service-manager",   "rust-userland", "/userland/service-manager",   "catalogued + supervisor state bridged"},
    {"diagnostics",       "rust-userland", "/userland/diagnostics",       "catalogued"},
    {"recovery",          "rust-userland", "/userland/recovery",          "catalogued"},
    {"update-manager",    "rust-userland", "/userland/update-manager",    "catalogued"},
    {"RustMyAdmin",       "rust-admin",    "/rustmyadmin",                "catalogued + /opt/rustmyadmin rootfs link"},
    {"dashboard",         "rust-admin",    "/dashboard",                  "catalogued"},
    {"dns-filter",        "rust-service",  "/services/dns-filter",        "catalogued"},
    {"firewall",          "rust-service",  "/services/firewall",          "catalogued + v46 firewall state bridged"},
    {"vpn",               "rust-service",  "/services/vpn",               "catalogued"},
    {"traffic-monitor",   "rust-service",  "/services/traffic-monitor",   "catalogued"},
    {"intrusion-watch",   "rust-service",  "/services/intrusion-watch",   "catalogued"},
    {"cloudflare-tunnel", "rust-service",  "/services/cloudflare-tunnel", "catalogued"},
    {"container-runner",  "rust-service",  "/services/container-runner",  "catalogued"},
    {"device-discovery",  "rust-service",  "/services/device-discovery",  "catalogued"},
    {"radio-manager",     "rust-service",  "/services/radio-manager",     "catalogued"},
    {"web-hosting",       "rust-service",  "/services/web-hosting",       "catalogued"},
    {"security",          "rust-security", "/security",                  "catalogued + v52 hardening gates frozen"},
};

static void con_putc(char c) {
    if (c == '\n') uart_putchar('\r');
    uart_putchar(c);
}

static void con_puts(const char *s) {
    if (!s) return;
    while (*s) con_putc(*s++);
}

static void con_u32_dec(u32 v) {
    char buf[12];
    u32 i = 0;
    if (v == 0) { con_putc('0'); return; }
    while (v && i < sizeof(buf)) {
        buf[i++] = (char)('0' + (v % 10U));
        v /= 10U;
    }
    while (i) con_putc(buf[--i]);
}

static void con_u32_hex(u32 v) {
    static const char hex[] = "0123456789abcdef";
    con_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        con_putc(hex[(v >> (u32)shift) & 0xFU]);
    }
}

static bool streq(const char *a, const char *b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

static bool starts_with(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    while (*prefix) {
        if (*s++ != *prefix++) return false;
    }
    return true;
}

static const char *skip_spaces(const char *s) {
    while (s && (*s == ' ' || *s == '\t')) s++;
    return s ? s : "";
}

static void trim_line(char *s) {
    u32 len = 0;
    while (s[len]) len++;
    while (len && (s[len - 1U] == ' ' || s[len - 1U] == '\t' || s[len - 1U] == '\r' || s[len - 1U] == '\n')) {
        s[--len] = '\0';
    }
}

static int con_readline(char *buf, u32 cap) {
    if (!buf || cap == 0) return -1;
    u32 len = 0;
    for (;;) {
        int ch = uart_getchar();
        if (ch < 0) {
            scheduler_yield();
            continue;
        }
        if (ch == '\r' || ch == '\n') {
            con_puts("\n");
            buf[len] = '\0';
            return (int)len;
        }
        if (ch == 0x7f || ch == '\b') {
            if (len) {
                len--;
                con_puts("\b \b");
            }
            continue;
        }
        if (ch < 0x20 || ch > 0x7e) continue;
        if (len + 1U < cap) {
            buf[len++] = (char)ch;
            con_putc((char)ch);
        }
    }
}

static void cmd_help(void) {
    con_puts("commands:\n");
    con_puts("  help                 show this help\n");
    con_puts("  status               release, mounts, userland, supervisor, network\n");
    con_puts("  services             kernel/userland service registry\n");
    con_puts("  rust                 Rust userland/service source bridge\n");
    con_puts("  rustmyadmin          RustMyAdmin integration status\n");
    con_puts("  net                  DHCP/NAT/firewall/router state\n");
    con_puts("  variants             Lite/Pro/Bastion/Router release image matrix\n");
    con_puts("  rootfs               mounted runtime/initramfs catalogue\n");
    con_puts("  ls [prefix]          list runtime files by prefix, e.g. ls /etc\n");
    con_puts("  cat <path>           print runtime text file, e.g. cat /etc/services.toml\n");
    con_puts("  run <name>           show how a service/source entry is wired\n");
    con_puts("  clear                clear screen\n");
    con_puts("  halt                 stop in the console loop\n");
}

static void cmd_status(void) {
    const aegis_release_final_state_t *rel = release_final_state();
    const aegis_userland_handoff_t *handoff = userland_handoff_state();
    const aegis_service_supervisor_state_t *sup = service_supervisor_state();
    const aegis_network_control_state_t *net = network_control_state();

    con_puts("AegisOS v2.0 v55.1 interactive runtime\n");
    con_puts("release-ready: "); con_puts(release_final_ready() ? "yes" : "no"); con_puts("\n");
    con_puts("release variants: "); con_u32_dec(rel ? rel->variant_count : 0); con_puts("\n");
    con_puts("mounts: "); con_u32_dec(vfs_mount_count()); con_puts(" root="); con_puts(vfs_mount_fs_name("/")); con_puts(" dev="); con_puts(vfs_mount_fs_name("/dev")); con_puts(" proc="); con_puts(vfs_mount_fs_name("/proc")); con_puts("\n");
    con_puts("initramfs files: "); con_u32_dec(initramfs_file_count()); con_puts("\n");
    con_puts("userland features: "); con_u32_dec(handoff ? handoff->feature_count : 0); con_puts(" ready="); con_u32_dec(handoff ? handoff->ready_count : 0); con_puts(" processes="); con_u32_dec(handoff ? handoff->process_descriptor_count : 0); con_puts("\n");
    con_puts("supervisor services: "); con_u32_dec(sup ? sup->service_count : 0); con_puts(" running="); con_u32_dec(sup ? sup->running_count : 0); con_puts(" faulted="); con_u32_dec(sup ? sup->faulted_count : 0); con_puts("\n");
    con_puts("network: "); con_puts(network_control_ready() ? "ready" : "not-ready"); con_puts(" lease="); con_u32_hex(net ? net->lease_ip : 0); con_puts(" nat="); con_puts(network_control_nat_ready() ? "on" : "off"); con_puts(" firewall="); con_puts(network_control_firewall_ready() ? "loaded" : "missing"); con_puts("\n");
}

static void cmd_services(void) {
    con_puts("kernel services:\n");
    u32 count = service_manager_count();
    for (u32 id = 1; id <= count; id++) {
        const aegis_service_t *s = service_manager_get(id);
        if (!s) continue;
        con_puts("  #"); con_u32_dec(s->id); con_puts(" "); con_puts(s->name); con_puts(" state="); con_puts(service_state_name(s->state)); con_puts(" deps="); con_u32_dec(s->dependency_count); con_puts("\n");
    }
    con_puts("\nsupervised runtime services:\n");
    const aegis_service_supervisor_state_t *state = service_supervisor_state();
    u32 max = state ? state->service_count : 0;
    for (u32 i = 1; i <= max; i++) {
        const aegis_supervised_service_t *svc = service_supervisor_get(i);
        if (!svc) continue;
        con_puts("  pid="); con_u32_dec(svc->pid); con_puts(" "); con_puts(svc->name); con_puts(" state="); con_puts(service_supervisor_state_name(svc->state)); con_puts(" faults="); con_u32_dec(svc->fault_count); con_puts("\n");
    }
    con_puts("\nuserland features:\n");
    for (u32 i = 1; i <= userland_feature_count(); i++) {
        const aegis_userland_feature_t *f = userland_get_feature(i);
        if (!f) continue;
        con_puts("  #"); con_u32_dec(f->id); con_puts(" "); con_puts(f->name); con_puts(" -> "); con_puts(f->image_path); con_puts(" state="); con_puts(userland_state_name(f->state)); con_puts("\n");
    }
}

static void cmd_rust(void) {
    con_puts("Rust/source bridge entries:\n");
    for (u32 i = 0; i < (u32)(sizeof(runtime_assets) / sizeof(runtime_assets[0])); i++) {
        con_puts("  "); con_puts(runtime_assets[i].name); con_puts(" ["); con_puts(runtime_assets[i].kind); con_puts("] "); con_puts(runtime_assets[i].path); con_puts(" — "); con_puts(runtime_assets[i].status); con_puts("\n");
    }
    con_puts("\nnote: v55.1 bridges/catalogues the Rust source trees into the runtime shell. Native Rust process execution still needs the final AegisOS Rust target + userspace ABI promotion.\n");
}

static void cmd_rustmyadmin(void) {
    const aegis_userland_feature_t *admin = userland_find_feature("rustmyadmin");
    con_puts("RustMyAdmin:\n");
    con_puts("  source: /rustmyadmin\n");
    con_puts("  rootfs: /opt/rustmyadmin\n");
    con_puts("  templates: dashboard/services/firewall/router/users/vpn/logs/backups\n");
    con_puts("  policy: /security/policy/rustmyadmin_policy.toml\n");
    con_puts("  service feature: "); con_puts(admin ? "present" : "missing");
    if (admin) { con_puts(" state="); con_puts(userland_state_name(admin->state)); con_puts(" image="); con_puts(admin->image_path); }
    con_puts("\n");
    con_puts("  status: connected to shell/catalogue; native web runtime waits on sockets/TCP + Rust userspace ABI.\n");
}

static void cmd_net(void) {
    const aegis_network_control_state_t *net = network_control_state();
    if (!net) { con_puts("network state unavailable\n"); return; }
    con_puts("network profile: "); con_puts(net->profile); con_puts("\n");
    con_puts("  WAN "); con_puts(net->wan_if); con_puts(" link="); con_puts(network_control_link_state_name(net->wan_link_state)); con_puts(" lease="); con_u32_hex(net->lease_ip); con_puts("\n");
    con_puts("  LAN "); con_puts(net->lan_if); con_puts("\n");
    con_puts("  gateway="); con_u32_hex(net->gateway); con_puts(" dns="); con_u32_hex(net->dns_server); con_puts(" routes="); con_u32_dec(net->route_count); con_puts("\n");
    con_puts("  NAT mappings="); con_u32_dec(net->nat_mapping_count); con_puts(" firewall rules="); con_u32_dec(net->firewall_rule_count); con_puts(" generation="); con_u32_dec(net->control_plane_generation); con_puts("\n");
}

static void cmd_variants(void) {
    const aegis_release_final_state_t *rel = release_final_state();
    if (!rel) { con_puts("release variant state unavailable\n"); return; }
    con_puts("release image variants:\n");
    for (u32 i = 0; i < rel->variant_count && i < AEGIS_RELEASE_VARIANT_MAX; i++) {
        con_puts("  "); con_puts(rel->variants[i].name); con_puts(" image="); con_puts(rel->variants[i].image_name); con_puts(" flash="); con_puts(rel->variants[i].flash_flow_hardened ? "yes" : "no"); con_puts(" checksum="); con_puts(rel->variants[i].checksum_declared ? "yes" : "no"); con_puts("\n");
    }
}

static void cmd_rootfs(void) {
    con_puts("runtime initramfs/rootfs entries:\n");
    for (u32 i = 0; i < initramfs_file_count(); i++) {
        const char *name = initramfs_file_name(i);
        if (!name) continue;
        con_puts("  /"); con_puts(name);
        if (initramfs_file_is_dir(i)) con_puts("/");
        con_puts("\n");
    }
}

static void cmd_ls(const char *arg) {
    const char *prefix = skip_spaces(arg);
    if (!prefix || prefix[0] == '\0') prefix = "/";
    if (prefix[0] == '/') prefix++;
    con_puts("listing /"); con_puts(prefix); con_puts("\n");
    for (u32 i = 0; i < initramfs_file_count(); i++) {
        const char *name = initramfs_file_name(i);
        if (!name) continue;
        if (prefix[0] == '\0' || starts_with(name, prefix)) {
            con_puts("  /"); con_puts(name);
            if (initramfs_file_is_dir(i)) con_puts("/");
            con_puts("\n");
        }
    }
}

static void cmd_cat(const char *arg) {
    const char *path = skip_spaces(arg);
    if (!path || path[0] == '\0') { con_puts("usage: cat <path>\n"); return; }
    const char *data = NULL;
    u64 size = 0;
    int rc = initramfs_read_text_file(path, &data, &size);
    if (rc != AEGIS_OK || !data) {
        con_puts("cat: not found or not a text file: "); con_puts(path); con_puts("\n");
        return;
    }
    for (u64 i = 0; i < size; i++) con_putc(data[i]);
    if (size == 0 || data[size - 1U] != '\n') con_puts("\n");
}

static void cmd_run(const char *arg) {
    const char *name = skip_spaces(arg);
    if (!name || name[0] == '\0') { con_puts("usage: run <service-or-asset-name>\n"); return; }
    for (u32 i = 0; i < (u32)(sizeof(runtime_assets) / sizeof(runtime_assets[0])); i++) {
        if (streq(name, runtime_assets[i].name)) {
            con_puts("run plan for "); con_puts(runtime_assets[i].name); con_puts(":\n");
            con_puts("  source path: "); con_puts(runtime_assets[i].path); con_puts("\n");
            con_puts("  kind: "); con_puts(runtime_assets[i].kind); con_puts("\n");
            con_puts("  status: "); con_puts(runtime_assets[i].status); con_puts("\n");
            con_puts("  execution: staged/catalogued in v55.1; native exec requires AegisOS Rust ABI/toolchain + scheduler promotion.\n");
            return;
        }
    }
    const aegis_userland_feature_t *f = userland_find_feature(name);
    if (f) {
        con_puts("userland feature "); con_puts(f->name); con_puts(" -> "); con_puts(f->image_path); con_puts(" state="); con_puts(userland_state_name(f->state)); con_puts("\n");
        return;
    }
    con_puts("run: unknown service/asset: "); con_puts(name); con_puts("\n");
}

static void console_banner(void) {
    con_puts("\n");
    con_puts("AegisOS v2.0 v55.1 runtime\n");
    con_puts("ttyAMA0 shell online. Type help. Ctrl+A X quits QEMU.\n\n");
}

void interactive_console_run(void) {
    char line[AEGIS_CONSOLE_LINE_MAX];
    console_banner();
    for (;;) {
        con_puts("aegis:/# ");
        int n = con_readline(line, sizeof(line));
        if (n < 0) continue;
        trim_line(line);
        const char *cmd = skip_spaces(line);
        if (cmd[0] == '\0') continue;
        if (streq(cmd, "help")) cmd_help();
        else if (streq(cmd, "status") || streq(cmd, "version")) cmd_status();
        else if (streq(cmd, "services")) cmd_services();
        else if (streq(cmd, "rust")) cmd_rust();
        else if (streq(cmd, "rustmyadmin")) cmd_rustmyadmin();
        else if (streq(cmd, "net") || streq(cmd, "network")) cmd_net();
        else if (streq(cmd, "variants")) cmd_variants();
        else if (streq(cmd, "rootfs")) cmd_rootfs();
        else if (starts_with(cmd, "ls")) cmd_ls(cmd + 2);
        else if (starts_with(cmd, "cat")) cmd_cat(cmd + 3);
        else if (starts_with(cmd, "run")) cmd_run(cmd + 3);
        else if (streq(cmd, "clear")) con_puts("\033[2J\033[H");
        else if (streq(cmd, "halt")) {
            con_puts("halted in AegisOS interactive console. Use Ctrl+A then X to quit QEMU.\n");
            for (;;) scheduler_yield();
        } else {
            con_puts("unknown command: "); con_puts(cmd); con_puts("\n");
        }
    }
}
