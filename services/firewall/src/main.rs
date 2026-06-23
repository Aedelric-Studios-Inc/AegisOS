//! AegisOS firewall service.
//!
//! Production path: install the hardened nftables baseline. It does not attempt
//! to reimplement packet filtering in userspace.

use std::io::Write;
use std::process::{Command, Stdio};
use std::thread;
use std::time::Duration;

fn main() {
    println!("[firewall] service started");
    match apply_nftables_baseline() {
        Ok(_) => println!("[firewall] nftables baseline loaded"),
        Err(e) => eprintln!("[firewall] baseline not loaded: {}", e),
    }

    loop {
        println!("[firewall] health nft={} baseline_loaded={}", command_available("nft"), baseline_loaded());
        thread::sleep(Duration::from_secs(60));
    }
}

fn apply_nftables_baseline() -> Result<(), String> {
    if dry_run() { return Ok(()); }
    if !command_available("nft") { return Err("nft not installed".to_owned()); }
    let script = std::fs::read_to_string("/etc/aegisos/nftables-router.nft")
        .unwrap_or_else(|_| default_nft_script());
    let mut child = Command::new("nft")
        .arg("-f")
        .arg("-")
        .stdin(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .map_err(|e| e.to_string())?;
    child.stdin.as_mut().ok_or("nft stdin unavailable")?
        .write_all(script.as_bytes())
        .map_err(|e| e.to_string())?;
    let out = child.wait_with_output().map_err(|e| e.to_string())?;
    if out.status.success() { Ok(()) } else { Err(String::from_utf8_lossy(&out.stderr).trim().to_owned()) }
}

fn default_nft_script() -> String {
    r#"
flush table inet aegis

table inet aegis {
  chain input {
    type filter hook input priority 0; policy drop;
    iifname "lo" accept
    ct state established,related accept
    iifname "eth1" tcp dport { 22, 8443 } accept
    iifname "eth1" udp dport { 53, 67 } accept
    ip protocol icmp accept
    counter log prefix "AEGIS-DROP-IN " drop
  }

  chain forward {
    type filter hook forward priority 0; policy drop;
    ct state established,related accept
    iifname "eth1" oifname { "eth0", "wwan0" } accept
    counter log prefix "AEGIS-DROP-FWD " drop
  }

  chain output {
    type filter hook output priority 0; policy accept;
  }
}
"#.to_owned()
}

fn baseline_loaded() -> bool {
    Command::new("nft").args(["list", "ruleset"]).output()
        .map(|out| out.status.success() && String::from_utf8_lossy(&out.stdout).contains("table inet aegis"))
        .unwrap_or(false)
}

fn command_available(cmd: &str) -> bool {
    Command::new("sh").arg("-c").arg(format!("command -v {} >/dev/null 2>&1", cmd)).status().map(|s| s.success()).unwrap_or(false)
}

fn dry_run() -> bool {
    std::env::var("AEGISOS_ROUTER_DRY_RUN").map(|v| v != "0").unwrap_or(false)
}
