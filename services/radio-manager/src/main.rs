//! aegis-radio-manager
//!
//! ÆÐELRIC Router/AegisBox LTE supervision backed by ModemManager and
//! NetworkManager. This is the carrier-module proving ground for PhoenixOS.

use std::process::Command;
use std::thread;
use std::time::Duration;

fn main() {
    eprintln!("[aegis-radio-manager] starting");
    loop {
        let status = radio_status();
        eprintln!("[aegis-radio-manager] {}", status);
        if std::env::var("AEGIS_RADIO_AUTOCONNECT").as_deref() == Ok("1") {
            ensure_lte_connection();
        }
        thread::sleep(Duration::from_secs(30));
    }
}

fn radio_status() -> String {
    let mm = command_available("mmcli");
    let nm = command_available("nmcli");
    let modems = if mm { modem_paths() } else { Vec::new() };
    let first = modems.first().cloned().unwrap_or_else(|| "none".to_owned());
    let state = if first != "none" { modem_state(&first) } else { "missing".to_owned() };
    format!(
        "backend=modemmanager available={} networkmanager={} modems={} primary={} state={}",
        mm, nm, modems.len(), first, state
    )
}

fn ensure_lte_connection() {
    if !command_available("nmcli") { return; }
    let con = std::env::var("AEGIS_RADIO_CONNECTION").unwrap_or_else(|_| "aegis-lte-giffgaff".to_owned());
    let active = Command::new("nmcli").args(["-t", "-f", "NAME", "connection", "show", "--active"])
        .output()
        .map(|out| String::from_utf8_lossy(&out.stdout).lines().any(|l| l == con))
        .unwrap_or(false);
    if !active {
        let _ = Command::new("nmcli").args(["connection", "up", &con]).status();
    }
}

fn modem_paths() -> Vec<String> {
    Command::new("mmcli").arg("-L").output()
        .ok()
        .filter(|out| out.status.success())
        .map(|out| String::from_utf8_lossy(&out.stdout).lines()
            .filter_map(|line| line.split_whitespace().find(|p| p.contains("/Modem/")))
            .map(|p| p.trim_matches(':').to_owned())
            .collect())
        .unwrap_or_default()
}

fn modem_state(path: &str) -> String {
    Command::new("mmcli").arg("-m").arg(path).output()
        .ok()
        .filter(|out| out.status.success())
        .and_then(|out| String::from_utf8_lossy(&out.stdout).lines()
            .find(|l| l.contains("state"))
            .map(|s| s.trim().to_owned()))
        .unwrap_or_else(|| "unknown".to_owned())
}

fn command_available(cmd: &str) -> bool {
    Command::new("sh")
        .arg("-c")
        .arg(format!("command -v {} >/dev/null 2>&1", shell_escape(cmd)))
        .status()
        .map(|s| s.success())
        .unwrap_or(false)
}

fn shell_escape(s: &str) -> String {
    s.chars().filter(|c| c.is_ascii_alphanumeric() || *c == '-' || *c == '_').collect()
}
