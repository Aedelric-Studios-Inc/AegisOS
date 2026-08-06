//! AegisOS VPN service.
//!
//! WireGuard is managed through wg/wg-quick. This service supervises the
//! configured interface; it does not fake or reimplement WireGuard packets.

use std::env;
use std::process::Command;
use std::thread;
use std::time::Duration;

const DEFAULT_WG_INTERFACE: &str = "wg0";

fn main() {
    let interface = env::var("AEGIS_VPN_INTERFACE")
        .unwrap_or_else(|_| DEFAULT_WG_INTERFACE.to_owned());
    println!("[vpn] service started, interface={interface}");
    let autostart = env::var("AEGIS_VPN_AUTOSTART").as_deref() == Ok("1");
    if autostart {
        wg_quick("up", &interface);
    }
    loop {
        println!("[vpn] status: {}", status_line(&interface));
        thread::sleep(Duration::from_secs(60));
    }
}

fn status_line(interface: &str) -> String {
    if !command_available("wg") {
        return "wg unavailable; install wireguard-tools".to_owned();
    }
    match Command::new("wg").args(["show", interface]).output() {
        Ok(output) if output.status.success() => {
            let text = String::from_utf8_lossy(&output.stdout);
            let peers = text
                .lines()
                .filter(|line| line.trim_start().starts_with("peer:"))
                .count();
            format!("interface={interface} active=true peers={peers}")
        }
        Ok(output) => format!(
            "interface={} active=false {}",
            interface,
            String::from_utf8_lossy(&output.stderr).trim()
        ),
        Err(error) => format!("interface={interface} active=false {error}"),
    }
}

fn wg_quick(action: &str, interface: &str) {
    if !command_available("wg-quick") {
        eprintln!("[vpn] wg-quick unavailable; install wireguard-tools");
        return;
    }
    if env::var("AEGISOS_ROUTER_DRY_RUN")
        .map(|value| value != "0")
        .unwrap_or(false)
    {
        return;
    }
    let status = Command::new("wg-quick").args([action, interface]).status();
    if !status.map(|value| value.success()).unwrap_or(false) {
        eprintln!("[vpn] wg-quick {action} {interface} failed");
    }
}

fn command_available(command: &str) -> bool {
    Command::new("sh")
        .arg("-c")
        .arg(format!("command -v {command} >/dev/null 2>&1"))
        .status()
        .map(|status| status.success())
        .unwrap_or(false)
}
