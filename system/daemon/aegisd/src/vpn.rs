//! VPN management for aegisd.
//!
//! v5 uses the real WireGuard tools (`wg`/`wg-quick`) instead of a fake packet
//! implementation. AegisOS manages WireGuard; it does not reimplement crypto.

use std::env;
use std::process::Command;
use crate::network;

const DEFAULT_WG_INTERFACE: &str = "wg0";

fn interface_name() -> String {
    env::var("AEGIS_VPN_INTERFACE").unwrap_or_else(|_| DEFAULT_WG_INTERFACE.to_owned())
}

pub fn get_status() -> String {
    format!(r#"{{"vpn":{}}}"#, status_object())
}

pub fn status_object() -> String {
    let interface = interface_name();
    let wg = network::command_available("wg");
    let wg_quick = network::command_available("wg-quick");
    if !wg {
        return format!(
            r#"{{"interface":"{}","backend":"wireguard-tools","wg_available":false,"wg_quick_available":{},"active":false,"peers":0,"state":"unavailable","error":"wireguard-tools not installed"}}"#,
            network::json_escape(&interface),
            wg_quick
        );
    }

    match Command::new("wg").args(["show", &interface]).output() {
        Ok(output) if output.status.success() => {
            let text = String::from_utf8_lossy(&output.stdout);
            let peers = text
                .lines()
                .filter(|line| line.trim_start().starts_with("peer:"))
                .count();
            let latest = text
                .lines()
                .find(|line| line.contains("latest handshake"))
                .unwrap_or("")
                .trim();
            format!(
                r#"{{"interface":"{}","backend":"wireguard-tools","wg_available":true,"wg_quick_available":{},"active":true,"state":"running","peers":{},"latest_handshake":"{}"}}"#,
                network::json_escape(&interface),
                wg_quick,
                peers,
                network::json_escape(latest)
            )
        }
        Ok(output) => {
            let error = String::from_utf8_lossy(&output.stderr);
            format!(
                r#"{{"interface":"{}","backend":"wireguard-tools","wg_available":true,"wg_quick_available":{},"active":false,"state":"stopped","peers":0,"error":"{}"}}"#,
                network::json_escape(&interface),
                wg_quick,
                network::json_escape(error.trim())
            )
        }
        Err(error) => format!(
            r#"{{"interface":"{}","backend":"wireguard-tools","wg_available":true,"wg_quick_available":{},"active":false,"state":"failed","error":"{}"}}"#,
            network::json_escape(&interface),
            wg_quick,
            network::json_escape(&error.to_string())
        ),
    }
}

pub fn up() -> String { wg_quick("up") }
pub fn down() -> String { wg_quick("down") }

fn wg_quick(action: &str) -> String {
    let interface = interface_name();
    if !network::command_available("wg-quick") {
        return format!(
            r#"{{"vpn_{}":{{"ok":false,"error":"wg-quick not installed"}}}}"#,
            action
        );
    }
    if network::dry_run() {
        return format!(
            r#"{{"vpn_{}":{{"ok":true,"dry_run":true,"interface":"{}"}}}}"#,
            action,
            network::json_escape(&interface)
        );
    }
    let ok = Command::new("wg-quick")
        .args([action, &interface])
        .status()
        .map(|status| status.success())
        .unwrap_or(false);
    format!(
        r#"{{"vpn_{}":{{"ok":{},"interface":"{}"}}}}"#,
        action,
        ok,
        network::json_escape(&interface)
    )
}
