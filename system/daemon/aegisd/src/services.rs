//! Service orchestration for aegisd.

use std::process::Command;

/// Well-known AegisOS service names.
const MANAGED_SERVICES: &[&str] = &[
    "aegis-vpn",
    "aegis-dns-filter",
    "aegis-firewall",
    "aegis-cloudflare-tunnel",
    "aegis-intrusion-watch",
    "aegis-traffic-monitor",
    "aegis-device-discovery",
    "aegis-radio-manager",
    "aegis-web-hosting",
    "aegis-container-runner",
];

pub fn list_services() -> String {
    let mut entries = Vec::new();
    for &name in MANAGED_SERVICES {
        let active = is_active(name);
        entries.push(format!(
            r#"{{"name":"{}","active":{}}}"#,
            json_escape(name),
            active
        ));
    }
    format!(r#"{{"services":[{}]}}"#, entries.join(","))
}

pub fn start_service(name: &str) -> String {
    if !MANAGED_SERVICES.contains(&name) {
        return format!(
            r#"{{"error":"unknown service","service":"{}"}}"#,
            json_escape(name)
        );
    }
    let ok = Command::new("systemctl")
        .args(["start", name])
        .status()
        .map(|s| s.success())
        .unwrap_or(false);
    format!(r#"{{"service":"{}","started":{}}}"#, json_escape(name), ok)
}

pub fn stop_service(name: &str) -> String {
    if !MANAGED_SERVICES.contains(&name) {
        return format!(
            r#"{{"error":"unknown service","service":"{}"}}"#,
            json_escape(name)
        );
    }
    let ok = Command::new("systemctl")
        .args(["stop", name])
        .status()
        .map(|s| s.success())
        .unwrap_or(false);
    format!(r#"{{"service":"{}","stopped":{}}}"#, json_escape(name), ok)
}

fn is_active(name: &str) -> bool {
    Command::new("systemctl")
        .args(["is-active", "--quiet", name])
        .status()
        .map(|s| s.success())
        .unwrap_or(false)
}

fn json_escape(s: &str) -> String {
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
