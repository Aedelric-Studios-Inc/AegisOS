//! AegisOS VPN service.
//!
//! WireGuard is managed through wg/wg-quick. This service supervises the
//! configured interface; it does not fake or reimplement WireGuard packets.

use std::process::Command;
use std::thread;
use std::time::Duration;

const WG_INTERFACE: &str = "wg0";

fn main() {
    println!("[vpn] service started");
    let autostart = std::env::var("AEGIS_VPN_AUTOSTART").as_deref() == Ok("1");
    if autostart {
        wg_quick("up");
    }
    loop {
        println!("[vpn] status: {}", status_line());
        thread::sleep(Duration::from_secs(60));
    }
}

fn status_line() -> String {
    if !command_available("wg") {
        return "wg unavailable".to_owned();
    }
    match Command::new("wg").args(["show", WG_INTERFACE]).output() {
        Ok(out) if out.status.success() => {
            let text = String::from_utf8_lossy(&out.stdout);
            let peers = text.lines().filter(|l| l.trim_start().starts_with("peer:")).count();
            format!("interface={} active=true peers={}", WG_INTERFACE, peers)
        }
        Ok(out) => format!("interface={} active=false {}", WG_INTERFACE, String::from_utf8_lossy(&out.stderr).trim()),
        Err(e) => format!("interface={} active=false {}", WG_INTERFACE, e),
    }
}

fn wg_quick(action: &str) {
    if !command_available("wg-quick") { return; }
    if std::env::var("AEGISOS_ROUTER_DRY_RUN").map(|v| v != "0").unwrap_or(false) { return; }
    let _ = Command::new("wg-quick").args([action, WG_INTERFACE]).status();
}

fn command_available(cmd: &str) -> bool {
    Command::new("sh").arg("-c").arg(format!("command -v {} >/dev/null 2>&1", cmd)).status().map(|s| s.success()).unwrap_or(false)
}
