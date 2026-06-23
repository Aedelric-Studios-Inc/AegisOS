//! LTE/SIM/radio telemetry and control for aegisd.
//!
//! Uses ModemManager (`mmcli`) and NetworkManager (`nmcli`) for the AegisBox
//! Router carrier module. This proves the radio abstraction PhoenixOS will need
//! without pretending AegisOS owns phone IMS/VoLTE yet.

use std::fs;
use std::process::Command;
use crate::network;

const RADIO_CONFIGS: &[&str] = &[
    "/etc/aegisos/radio.toml",
    "/etc/radio.toml",
    "system/config/radio.toml",
    "image/rootfs/etc/radio.toml",
];

#[derive(Clone, Debug)]
struct CarrierProfile {
    name: String,
    apn: String,
    username: String,
    password: String,
}

pub fn get_status() -> String {
    let modemmanager = network::command_available("mmcli");
    let networkmanager = network::command_available("nmcli");
    let modems = if modemmanager { modem_paths() } else { Vec::new() };
    let primary = load_primary_carrier();
    let first = modems.first().cloned().unwrap_or_else(|| "".to_owned());
    let detail = if first.is_empty() { "no modem".to_owned() } else { modem_summary(&first) };

    format!(
        r#"{{"radio":{{"backend":"{}","network_backend":"{}","modems_detected":{},"primary_modem":"{}","carrier_profile":"{}","apn":"{}","detail":"{}","ready":{}}}}}"#,
        if modemmanager { "modemmanager" } else { "unavailable" },
        if networkmanager { "networkmanager" } else { "unavailable" },
        modems.len(),
        network::json_escape(&first),
        network::json_escape(&primary.name),
        network::json_escape(&primary.apn),
        network::json_escape(&detail),
        modemmanager && networkmanager && !modems.is_empty()
    )
}

pub fn connect() -> String {
    let profile = load_primary_carrier();
    if !network::command_available("nmcli") {
        return r#"{"radio_connect":{"ok":false,"error":"nmcli not installed"}}"#.to_owned();
    }
    if network::dry_run() {
        return format!(r#"{{"radio_connect":{{"ok":true,"dry_run":true,"carrier":"{}","apn":"{}"}}}}"#, network::json_escape(&profile.name), network::json_escape(&profile.apn));
    }

    let connection = format!("aegis-lte-{}", sanitize(&profile.name));
    let _ = Command::new("nmcli").args(["connection", "delete", &connection]).status();
    let mut args = vec![
        "connection".to_owned(), "add".to_owned(),
        "type".to_owned(), "gsm".to_owned(),
        "ifname".to_owned(), "*".to_owned(),
        "con-name".to_owned(), connection.clone(),
        "apn".to_owned(), profile.apn.clone(),
        "connection.autoconnect".to_owned(), "yes".to_owned(),
    ];
    if !profile.username.is_empty() {
        args.extend(["gsm.username".to_owned(), profile.username.clone()]);
    }
    if !profile.password.is_empty() {
        args.extend(["gsm.password".to_owned(), profile.password.clone()]);
    }
    let ok = Command::new("nmcli").args(args.iter().map(String::as_str)).status().map(|s| s.success()).unwrap_or(false);
    let up = if ok {
        Command::new("nmcli").args(["connection", "up", &connection]).status().map(|s| s.success()).unwrap_or(false)
    } else { false };
    format!(r#"{{"radio_connect":{{"ok":{},"connection":"{}","carrier":"{}"}}}}"#, ok && up, network::json_escape(&connection), network::json_escape(&profile.name))
}

pub fn disconnect() -> String {
    if !network::command_available("nmcli") {
        return r#"{"radio_disconnect":{"ok":false,"error":"nmcli not installed"}}"#.to_owned();
    }
    let profile = load_primary_carrier();
    let connection = format!("aegis-lte-{}", sanitize(&profile.name));
    if network::dry_run() {
        return format!(r#"{{"radio_disconnect":{{"ok":true,"dry_run":true,"connection":"{}"}}}}"#, connection);
    }
    let ok = Command::new("nmcli").args(["connection", "down", &connection]).status().map(|s| s.success()).unwrap_or(false);
    format!(r#"{{"radio_disconnect":{{"ok":{},"connection":"{}"}}}}"#, ok, network::json_escape(&connection))
}

fn modem_paths() -> Vec<String> {
    let output = Command::new("mmcli").arg("-L").output();
    match output {
        Ok(out) if out.status.success() => {
            let text = String::from_utf8_lossy(&out.stdout);
            text.lines()
                .filter_map(|line| line.split_whitespace().find(|p| p.contains("/Modem/")))
                .map(|p| p.trim_matches(':').to_owned())
                .collect()
        }
        _ => Vec::new(),
    }
}

fn modem_summary(path: &str) -> String {
    Command::new("mmcli").arg("-m").arg(path).output()
        .ok()
        .filter(|out| out.status.success())
        .map(|out| String::from_utf8_lossy(&out.stdout).lines().take(12).collect::<Vec<_>>().join(" | "))
        .unwrap_or_else(|| "modem detail unavailable".to_owned())
}

fn load_primary_carrier() -> CarrierProfile {
    let text = read_first_config(RADIO_CONFIGS).unwrap_or_default();
    CarrierProfile {
        name: value(&text, "primary_carrier").or_else(|| value(&text, "name")).unwrap_or("giffgaff").to_owned(),
        apn: value(&text, "apn").unwrap_or("giffgaff.com").to_owned(),
        username: value(&text, "username").unwrap_or("giffgaff").to_owned(),
        password: value(&text, "password").unwrap_or("").to_owned(),
    }
}

fn read_first_config(paths: &[&str]) -> Option<String> {
    for p in paths {
        if let Ok(s) = fs::read_to_string(p) { return Some(s); }
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

fn sanitize(s: &str) -> String {
    s.chars().filter(|c| c.is_ascii_alphanumeric() || *c == '-' || *c == '_').collect()
}
