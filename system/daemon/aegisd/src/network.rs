//! Router network backend for aegisd.
//!
//! This is the AegisBox/ÆÐELRIC Router bring-up layer. It uses standard Linux
//! router tools (NetworkManager, ModemManager, nftables, dnsmasq, WireGuard)
//! rather than pretending the custom kernel already owns every driver path.
//! PhoenixOS can later reuse the same radio/network abstractions with a phone UI.

use std::fs;
use std::process::Command;

const NETWORK_CONFIGS: &[&str] = &[
    "/etc/aegisos/network.toml",
    "/etc/network.toml",
    "system/config/network.toml",
    "image/rootfs/etc/network.toml",
];

#[derive(Clone, Debug)]
pub struct RouterConfig {
    pub wan: String,
    pub lan: String,
    pub wifi: String,
    pub wwan: String,
    pub bluetooth: String,
    pub lan_address: String,
    pub lan_gateway: String,
    pub dhcp_start: String,
    pub dhcp_end: String,
    pub dns_listen: String,
}

impl RouterConfig {
    pub fn load() -> Self {
        let text = read_first_config(NETWORK_CONFIGS).unwrap_or_default();
        Self {
            wan: value(&text, "wan").unwrap_or("eth0").to_owned(),
            lan: value(&text, "lan").unwrap_or("eth1").to_owned(),
            wifi: value(&text, "wifi").unwrap_or("wlan0").to_owned(),
            wwan: value(&text, "wwan").unwrap_or("wwan0").to_owned(),
            bluetooth: value(&text, "bluetooth").unwrap_or("bt0").to_owned(),
            lan_address: value(&text, "lan_address").unwrap_or("192.168.88.1/24").to_owned(),
            lan_gateway: value(&text, "lan_gateway").unwrap_or("192.168.88.1").to_owned(),
            dhcp_start: value(&text, "dhcp_start").unwrap_or("192.168.88.100").to_owned(),
            dhcp_end: value(&text, "dhcp_end").unwrap_or("192.168.88.240").to_owned(),
            dns_listen: value(&text, "dns_listen").unwrap_or("192.168.88.1").to_owned(),
        }
    }
}

pub fn get_interfaces() -> String {
    let cfg = RouterConfig::load();
    let entries = [
        iface_json("wan", &cfg.wan),
        iface_json("lan", &cfg.lan),
        iface_json("wifi", &cfg.wifi),
        iface_json("wwan", &cfg.wwan),
        iface_json("bluetooth", &cfg.bluetooth),
    ];
    format!(
        r#"{{"interfaces":[{}],"lan_address":"{}","dhcp_range":"{}-{}","dns_listen":"{}"}}"#,
        entries.join(","),
        json_escape(&cfg.lan_address),
        json_escape(&cfg.dhcp_start),
        json_escape(&cfg.dhcp_end),
        json_escape(&cfg.dns_listen),
    )
}

pub fn apply_network_profiles() -> String {
    let cfg = RouterConfig::load();
    let mut actions = Vec::new();

    actions.push(nmcli_apply_lan(&cfg));
    actions.push(nmcli_apply_wan(&cfg.wan));
    actions.push(enable_ipv4_forwarding());

    format!(r#"{{"network_apply":[{}]}}"#, actions.join(","))
}

pub fn router_status() -> String {
    let cfg = RouterConfig::load();
    let forwarding = read_trim("/proc/sys/net/ipv4/ip_forward").unwrap_or_else(|| "unknown".to_owned());
    let nm = command_available("nmcli");
    let mm = command_available("mmcli");
    let nft = command_available("nft");
    let dnsmasq = command_available("dnsmasq");
    let wg = command_available("wg");

    format!(
        r#"{{"router":{{"ready":{},"ipv4_forwarding":"{}","tools":{{"nmcli":{},"mmcli":{},"nft":{},"dnsmasq":{},"wg":{}}},"roles":{{"wan":"{}","lan":"{}","wifi":"{}","wwan":"{}","bluetooth":"{}"}},"lan_address":"{}","dhcp_range":"{}-{}"}}}}"#,
        nm && nft,
        json_escape(&forwarding),
        nm, mm, nft, dnsmasq, wg,
        json_escape(&cfg.wan),
        json_escape(&cfg.lan),
        json_escape(&cfg.wifi),
        json_escape(&cfg.wwan),
        json_escape(&cfg.bluetooth),
        json_escape(&cfg.lan_address),
        json_escape(&cfg.dhcp_start),
        json_escape(&cfg.dhcp_end),
    )
}

fn iface_json(role: &str, name: &str) -> String {
    let operstate = read_trim(&format!("/sys/class/net/{}/operstate", name))
        .unwrap_or_else(|| "missing".to_owned());
    let carrier = read_trim(&format!("/sys/class/net/{}/carrier", name))
        .unwrap_or_else(|| "unknown".to_owned());
    format!(
        r#"{{"role":"{}","name":"{}","operstate":"{}","carrier":"{}","present":{}}}"#,
        json_escape(role),
        json_escape(name),
        json_escape(&operstate),
        json_escape(&carrier),
        operstate != "missing"
    )
}

fn nmcli_apply_lan(cfg: &RouterConfig) -> String {
    if !command_available("nmcli") {
        return action_json("lan-profile", false, "nmcli not installed");
    }
    if dry_run() {
        return action_json("lan-profile", true, "dry-run");
    }

    let connection = "aegis-lan";
    let _ = Command::new("nmcli").args(["connection", "delete", connection]).status();
    let add = Command::new("nmcli").args([
        "connection", "add",
        "type", "ethernet",
        "con-name", connection,
        "ifname", &cfg.lan,
        "ipv4.method", "manual",
        "ipv4.addresses", &cfg.lan_address,
        "ipv4.gateway", &cfg.lan_gateway,
        "ipv6.method", "ignore",
        "connection.autoconnect", "yes",
    ]).status().map(|s| s.success()).unwrap_or(false);
    if add {
        let _ = Command::new("nmcli").args(["connection", "up", connection]).status();
    }
    action_json("lan-profile", add, if add { "configured" } else { "nmcli failed" })
}

fn nmcli_apply_wan(wan: &str) -> String {
    if !command_available("nmcli") {
        return action_json("wan-profile", false, "nmcli not installed");
    }
    if dry_run() {
        return action_json("wan-profile", true, "dry-run");
    }

    let connection = "aegis-wan";
    let _ = Command::new("nmcli").args(["connection", "delete", connection]).status();
    let ok = Command::new("nmcli").args([
        "connection", "add",
        "type", "ethernet",
        "con-name", connection,
        "ifname", wan,
        "ipv4.method", "auto",
        "ipv6.method", "auto",
        "connection.autoconnect", "yes",
    ]).status().map(|s| s.success()).unwrap_or(false);
    if ok {
        let _ = Command::new("nmcli").args(["connection", "up", connection]).status();
    }
    action_json("wan-profile", ok, if ok { "configured" } else { "nmcli failed" })
}

fn enable_ipv4_forwarding() -> String {
    if dry_run() {
        return action_json("ipv4-forwarding", true, "dry-run");
    }
    match fs::write("/proc/sys/net/ipv4/ip_forward", "1\n") {
        Ok(_) => action_json("ipv4-forwarding", true, "enabled"),
        Err(e) => action_json("ipv4-forwarding", false, &e.to_string()),
    }
}

pub fn command_available(cmd: &str) -> bool {
    Command::new("sh")
        .arg("-c")
        .arg(format!("command -v {} >/dev/null 2>&1", shell_token(cmd)))
        .status()
        .map(|s| s.success())
        .unwrap_or(false)
}

pub fn dry_run() -> bool {
    std::env::var("AEGISOS_ROUTER_DRY_RUN").map(|v| v != "0").unwrap_or(false)
}

pub fn action_json(action: &str, ok: bool, detail: &str) -> String {
    format!(
        r#"{{"action":"{}","ok":{},"detail":"{}"}}"#,
        json_escape(action), ok, json_escape(detail)
    )
}

fn read_first_config(paths: &[&str]) -> Option<String> {
    for p in paths {
        if let Ok(s) = fs::read_to_string(p) {
            return Some(s);
        }
    }
    None
}

fn value<'a>(text: &'a str, key: &str) -> Option<&'a str> {
    let prefix = format!("{} =", key);
    for line in text.lines() {
        let line = line.trim();
        if line.starts_with('#') || line.is_empty() { continue; }
        if let Some(rest) = line.strip_prefix(&prefix) {
            return Some(rest.trim().trim_matches('"'));
        }
    }
    None
}

fn read_trim(path: &str) -> Option<String> {
    fs::read_to_string(path).ok().map(|s| s.trim().to_owned())
}

fn shell_token(s: &str) -> String {
    s.chars().filter(|c| c.is_ascii_alphanumeric() || *c == '-' || *c == '_' || *c == '.').collect()
}

pub fn json_escape(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    for ch in s.chars() {
        match ch {
            '\\' => out.push_str("\\\\"),
            '"' => out.push_str("\\\""),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            c if c.is_control() => out.push_str(&format!("\\u{:04x}", c as u32)),
            c => out.push(c),
        }
    }
    out
}
