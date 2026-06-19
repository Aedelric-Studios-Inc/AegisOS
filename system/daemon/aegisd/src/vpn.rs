//! VPN management for aegisd.

use std::process::Command;

const WG_INTERFACE: &str = "wg0";

pub fn get_status() -> String {
    // Query WireGuard interface status via `wg show`.
    let output = Command::new("wg")
        .args(["show", WG_INTERFACE])
        .output();

    match output {
        Ok(out) if out.status.success() => {
            let text = String::from_utf8_lossy(&out.stdout);
            let peers = text.lines()
                .filter(|l| l.trim_start().starts_with("peer:"))
                .count();
            format!(r#"{{"interface":"{}","active":true,"peers":{}}}"#, WG_INTERFACE, peers)
        }
        Ok(_) => {
            format!(r#"{{"interface":"{}","active":false,"peers":0}}"#, WG_INTERFACE)
        }
        Err(e) => {
            format!(r#"{{"interface":"{}","active":false,"error":"{}"}}"#, WG_INTERFACE, e)
        }
    }
}
