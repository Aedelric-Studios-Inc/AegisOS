//! Router network backend for aegisd.
//!
//! The production profile uses fixed AegisBox interface names. The development
//! profile can discover the host's real default-route and wireless interfaces,
//! while creating a dedicated `aegis-lan0` bridge instead of rewriting the
//! laptop's active WAN connection.

use std::fs;
use std::path::Path;
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
    pub lan_subnet: String,
    pub lan_gateway: String,
    pub dhcp_start: String,
    pub dhcp_end: String,
    pub dns_listen: String,
    pub auto_discover: bool,
    pub manage_wan: bool,
}

impl RouterConfig {
    pub fn load() -> Self {
        let text = read_network_config().unwrap_or_default();
        let auto_discover = bool_value(&text, "auto_discover").unwrap_or(false);
        let manage_wan = bool_value(&text, "manage_wan").unwrap_or(true);

        let mut config = Self {
            wan: value(&text, "wan").unwrap_or("eth0").to_owned(),
            lan: value(&text, "lan").unwrap_or("eth1").to_owned(),
            wifi: value(&text, "wifi").unwrap_or("wlan0").to_owned(),
            wwan: value(&text, "wwan").unwrap_or("wwan0").to_owned(),
            bluetooth: value(&text, "bluetooth").unwrap_or("bt0").to_owned(),
            lan_address: value(&text, "lan_address")
                .unwrap_or("192.168.88.1/24")
                .to_owned(),
            lan_subnet: value(&text, "lan_subnet")
                .unwrap_or("192.168.88.0/24")
                .to_owned(),
            lan_gateway: value(&text, "lan_gateway")
                .unwrap_or("192.168.88.1")
                .to_owned(),
            dhcp_start: value(&text, "dhcp_start")
                .unwrap_or("192.168.88.100")
                .to_owned(),
            dhcp_end: value(&text, "dhcp_end")
                .unwrap_or("192.168.88.240")
                .to_owned(),
            dns_listen: value(&text, "dns_listen")
                .unwrap_or("192.168.88.1")
                .to_owned(),
            auto_discover,
            manage_wan,
        };

        if config.auto_discover {
            config.resolve_host_interfaces();
        }
        config
    }

    fn resolve_host_interfaces(&mut self) {
        let interfaces = interface_names();
        let default_route = default_route_interface();
        let wireless = interfaces
            .iter()
            .find(|name| is_wireless(name))
            .cloned();
        let mobile = interfaces
            .iter()
            .find(|name| is_mobile_interface(name))
            .cloned();
        let wired = interfaces
            .iter()
            .filter(|name| !is_wireless(name) && !is_mobile_interface(name))
            .cloned()
            .collect::<Vec<_>>();

        if self.wan == "auto" || !interface_present(&self.wan) {
            self.wan = default_route
                .or_else(|| wired.first().cloned())
                .or(wireless.clone())
                .unwrap_or_else(|| "unavailable".to_owned());
        }

        if self.wifi == "auto" || !interface_present(&self.wifi) {
            self.wifi = wireless.unwrap_or_else(|| "unavailable".to_owned());
        }

        if self.wwan == "auto" || !interface_present(&self.wwan) {
            self.wwan = mobile.unwrap_or_else(|| "unavailable".to_owned());
        }

        if self.lan == "auto" {
            self.lan = wired
                .into_iter()
                .find(|name| name != &self.wan)
                .unwrap_or_else(|| "aegis-lan0".to_owned());
        } else if !interface_present(&self.lan) && !self.lan.starts_with("aegis-") {
            self.lan = "aegis-lan0".to_owned();
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
        r#"{{"interfaces":[{}],"lan_address":"{}","lan_subnet":"{}","dhcp_range":"{}-{}","dns_listen":"{}","auto_discover":{},"manage_wan":{}}}"#,
        entries.join(","),
        json_escape(&cfg.lan_address),
        json_escape(&cfg.lan_subnet),
        json_escape(&cfg.dhcp_start),
        json_escape(&cfg.dhcp_end),
        json_escape(&cfg.dns_listen),
        cfg.auto_discover,
        cfg.manage_wan,
    )
}

pub fn apply_network_profiles() -> String {
    let cfg = RouterConfig::load();
    let mut actions = Vec::new();

    actions.push(nmcli_apply_lan(&cfg));
    actions.push(if cfg.manage_wan {
        nmcli_apply_wan(&cfg.wan)
    } else {
        action_json("wan-profile", true, "preserved existing host WAN profile")
    });
    actions.push(enable_ipv4_forwarding());

    format!(r#"{{"network_apply":[{}]}}"#, actions.join(","))
}

pub fn router_status() -> String {
    let cfg = RouterConfig::load();
    let forwarding = read_trim("/proc/sys/net/ipv4/ip_forward")
        .unwrap_or_else(|| "unknown".to_owned());
    let nm = command_available("nmcli");
    let mm = command_available("mmcli");
    let nft = command_available("nft");
    let dnsmasq = command_available("dnsmasq");
    let wg = command_available("wg");
    let wan_present = interface_present(&cfg.wan);
    let lan_present = interface_present(&cfg.lan);
    let tooling_ready = nm && nft && dnsmasq;
    let interfaces_ready = wan_present && lan_present;
    let ready = tooling_ready && interfaces_ready && forwarding == "1";

    format!(
        r#"{{"router":{{"ready":{},"tooling_ready":{},"interfaces_ready":{},"ipv4_forwarding":"{}","tools":{{"nmcli":{},"mmcli":{},"nft":{},"dnsmasq":{},"wg":{}}},"roles":{{"wan":"{}","lan":"{}","wifi":"{}","wwan":"{}","bluetooth":"{}"}},"role_presence":{{"wan":{},"lan":{}}},"lan_address":"{}","lan_subnet":"{}","dhcp_range":"{}-{}","auto_discover":{},"manage_wan":{}}}}}"#,
        ready,
        tooling_ready,
        interfaces_ready,
        json_escape(&forwarding),
        nm,
        mm,
        nft,
        dnsmasq,
        wg,
        json_escape(&cfg.wan),
        json_escape(&cfg.lan),
        json_escape(&cfg.wifi),
        json_escape(&cfg.wwan),
        json_escape(&cfg.bluetooth),
        wan_present,
        lan_present,
        json_escape(&cfg.lan_address),
        json_escape(&cfg.lan_subnet),
        json_escape(&cfg.dhcp_start),
        json_escape(&cfg.dhcp_end),
        cfg.auto_discover,
        cfg.manage_wan,
    )
}

pub fn interface_present(name: &str) -> bool {
    !name.is_empty() && Path::new(&format!("/sys/class/net/{name}")).exists()
}

fn iface_json(role: &str, name: &str) -> String {
    let operstate = read_trim(&format!("/sys/class/net/{name}/operstate"))
        .unwrap_or_else(|| "missing".to_owned());
    let carrier = read_trim(&format!("/sys/class/net/{name}/carrier"))
        .unwrap_or_else(|| "unknown".to_owned());
    format!(
        r#"{{"role":"{}","name":"{}","operstate":"{}","carrier":"{}","present":{}}}"#,
        json_escape(role),
        json_escape(name),
        json_escape(&operstate),
        json_escape(&carrier),
        interface_present(name)
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
    let _ = Command::new("nmcli")
        .args(["connection", "delete", connection])
        .status();

    let kind = if cfg.lan.starts_with("aegis-") {
        "bridge"
    } else if interface_present(&cfg.lan) {
        "ethernet"
    } else {
        return action_json("lan-profile", false, "configured LAN interface is missing");
    };

    let add = Command::new("nmcli")
        .args([
            "connection",
            "add",
            "type",
            kind,
            "con-name",
            connection,
            "ifname",
            &cfg.lan,
            "ipv4.method",
            "manual",
            "ipv4.addresses",
            &cfg.lan_address,
            "ipv6.method",
            "disabled",
            "connection.autoconnect",
            "yes",
        ])
        .status()
        .map(|status| status.success())
        .unwrap_or(false);
    let up = add
        && Command::new("nmcli")
            .args(["connection", "up", connection])
            .status()
            .map(|status| status.success())
            .unwrap_or(false);

    action_json(
        "lan-profile",
        add && up,
        if add && up {
            if kind == "bridge" {
                "development LAN bridge configured"
            } else {
                "LAN interface configured"
            }
        } else {
            "nmcli failed"
        },
    )
}

fn nmcli_apply_wan(wan: &str) -> String {
    if !command_available("nmcli") {
        return action_json("wan-profile", false, "nmcli not installed");
    }
    if !interface_present(wan) {
        return action_json("wan-profile", false, "configured WAN interface is missing");
    }
    if dry_run() {
        return action_json("wan-profile", true, "dry-run");
    }

    let connection = "aegis-wan";
    let _ = Command::new("nmcli")
        .args(["connection", "delete", connection])
        .status();
    let ok = Command::new("nmcli")
        .args([
            "connection",
            "add",
            "type",
            "ethernet",
            "con-name",
            connection,
            "ifname",
            wan,
            "ipv4.method",
            "auto",
            "ipv6.method",
            "auto",
            "connection.autoconnect",
            "yes",
        ])
        .status()
        .map(|status| status.success())
        .unwrap_or(false);
    let up = ok
        && Command::new("nmcli")
            .args(["connection", "up", connection])
            .status()
            .map(|status| status.success())
            .unwrap_or(false);
    action_json(
        "wan-profile",
        ok && up,
        if ok && up { "configured" } else { "nmcli failed" },
    )
}

fn enable_ipv4_forwarding() -> String {
    if dry_run() {
        return action_json("ipv4-forwarding", true, "dry-run");
    }
    match fs::write("/proc/sys/net/ipv4/ip_forward", "1\n") {
        Ok(_) => action_json("ipv4-forwarding", true, "enabled"),
        Err(error) => action_json("ipv4-forwarding", false, &error.to_string()),
    }
}

fn interface_names() -> Vec<String> {
    let mut names = fs::read_dir("/sys/class/net")
        .ok()
        .into_iter()
        .flatten()
        .filter_map(Result::ok)
        .filter_map(|entry| entry.file_name().into_string().ok())
        .filter(|name| name != "lo")
        .collect::<Vec<_>>();
    names.sort();
    names
}

fn default_route_interface() -> Option<String> {
    let routes = fs::read_to_string("/proc/net/route").ok()?;
    routes.lines().skip(1).find_map(|line| {
        let fields = line.split_whitespace().collect::<Vec<_>>();
        (fields.len() > 3 && fields[1] == "00000000").then(|| fields[0].to_owned())
    })
}

fn is_wireless(name: &str) -> bool {
    Path::new(&format!("/sys/class/net/{name}/wireless")).exists()
}

fn is_mobile_interface(name: &str) -> bool {
    name.starts_with("wwan")
        || name.starts_with("wwp")
        || name.starts_with("cdc")
        || name.starts_with("usb")
}

pub fn command_available(cmd: &str) -> bool {
    Command::new("sh")
        .arg("-c")
        .arg(format!("command -v {} >/dev/null 2>&1", shell_token(cmd)))
        .status()
        .map(|status| status.success())
        .unwrap_or(false)
}

pub fn dry_run() -> bool {
    std::env::var("AEGISOS_ROUTER_DRY_RUN")
        .map(|value| value != "0")
        .unwrap_or(false)
}

pub fn action_json(action: &str, ok: bool, detail: &str) -> String {
    format!(
        r#"{{"action":"{}","ok":{},"detail":"{}"}}"#,
        json_escape(action),
        ok,
        json_escape(detail)
    )
}

fn read_network_config() -> Option<String> {
    if let Ok(path) = std::env::var("AEGISOS_NETWORK_CONFIG") {
        if let Ok(content) = fs::read_to_string(path) {
            return Some(content);
        }
    }
    read_first_config(NETWORK_CONFIGS)
}

fn read_first_config(paths: &[&str]) -> Option<String> {
    for path in paths {
        if let Ok(content) = fs::read_to_string(path) {
            return Some(content);
        }
    }
    None
}

fn value<'a>(text: &'a str, key: &str) -> Option<&'a str> {
    let prefix = format!("{key} =");
    for line in text.lines() {
        let line = line.trim();
        if line.starts_with('#') || line.is_empty() {
            continue;
        }
        if let Some(rest) = line.strip_prefix(&prefix) {
            return Some(rest.trim().trim_matches('"'));
        }
    }
    None
}

fn bool_value(text: &str, key: &str) -> Option<bool> {
    match value(text, key)? {
        "true" => Some(true),
        "false" => Some(false),
        _ => None,
    }
}

fn read_trim(path: &str) -> Option<String> {
    fs::read_to_string(path).ok().map(|content| content.trim().to_owned())
}

fn shell_token(value: &str) -> String {
    value
        .chars()
        .filter(|character| {
            character.is_ascii_alphanumeric() || matches!(character, '-' | '_' | '.')
        })
        .collect()
}

pub fn json_escape(value: &str) -> String {
    let mut output = String::with_capacity(value.len());
    for character in value.chars() {
        match character {
            '\\' => output.push_str("\\\\"),
            '"' => output.push_str("\\\""),
            '\n' => output.push_str("\\n"),
            '\r' => output.push_str("\\r"),
            '\t' => output.push_str("\\t"),
            control if control.is_control() => {
                output.push_str(&format!("\\u{:04x}", control as u32));
            }
            other => output.push(other),
        }
    }
    output
}
