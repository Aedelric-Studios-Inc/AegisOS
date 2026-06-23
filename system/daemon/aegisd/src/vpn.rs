//! VPN management for aegisd.
//!
//! v5 uses the real WireGuard tools (`wg`/`wg-quick`) instead of a fake packet
//! implementation. AegisOS manages WireGuard; it does not reimplement crypto.

use std::process::Command;
use crate::network;

const WG_INTERFACE: &str = "wg0";

pub fn get_status() -> String {
    format!(r#"{{"vpn":{}}}"#, status_object())
}

pub fn status_object() -> String {
    let wg = network::command_available("wg");
    let wg_quick = network::command_available("wg-quick");
    let output = Command::new("wg").args(["show", WG_INTERFACE]).output();

    match output {
        Ok(out) if out.status.success() => {
            let text = String::from_utf8_lossy(&out.stdout);
            let peers = text.lines().filter(|l| l.trim_start().starts_with("peer:")).count();
            let latest = text.lines().find(|l| l.contains("latest handshake")).unwrap_or("").trim();
            format!(
                r#"{{"interface":"{}","backend":"wireguard-tools","wg_available":{},"wg_quick_available":{},"active":true,"peers":{},"latest_handshake":"{}"}}"#,
                WG_INTERFACE, wg, wg_quick, peers, network::json_escape(latest)
            )
        }
        Ok(out) => {
            let err = String::from_utf8_lossy(&out.stderr);
            format!(
                r#"{{"interface":"{}","backend":"wireguard-tools","wg_available":{},"wg_quick_available":{},"active":false,"peers":0,"error":"{}"}}"#,
                WG_INTERFACE, wg, wg_quick, network::json_escape(err.trim())
            )
        }
        Err(e) => {
            format!(
                r#"{{"interface":"{}","backend":"wireguard-tools","wg_available":{},"wg_quick_available":{},"active":false,"error":"{}"}}"#,
                WG_INTERFACE, wg, wg_quick, network::json_escape(&e.to_string())
            )
        }
    }
}

pub fn up() -> String { wg_quick("up") }
pub fn down() -> String { wg_quick("down") }

fn wg_quick(action: &str) -> String {
    if !network::command_available("wg-quick") {
        return format!(r#"{{"vpn_{}":{{"ok":false,"error":"wg-quick not installed"}}}}"#, action);
    }
    if network::dry_run() {
        return format!(r#"{{"vpn_{}":{{"ok":true,"dry_run":true,"interface":"{}"}}}}"#, action, WG_INTERFACE);
    }
    let ok = Command::new("wg-quick").args([action, WG_INTERFACE]).status().map(|s| s.success()).unwrap_or(false);
    format!(r#"{{"vpn_{}":{{"ok":{},"interface":"{}"}}}}"#, action, ok, WG_INTERFACE)
}
