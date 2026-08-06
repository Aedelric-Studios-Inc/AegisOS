//! AegisOS firewall service.
//!
//! The service keeps nftables health visible and can load the canonical ruleset.
//! In development the router provisioner loads the generated host-safe ruleset,
//! so `AEGIS_FIREWALL_SKIP_APPLY=1` prevents a second conflicting apply.

use std::io::Write;
use std::process::{Command, Stdio};
use std::thread;
use std::time::Duration;

const DEFAULT_RULESET_PATH: &str = "/etc/aegisos/nftables-router.nft";

fn main() {
    println!("[firewall] service started");
    if std::env::var("AEGIS_FIREWALL_SKIP_APPLY").as_deref() == Ok("1") {
        println!("[firewall] baseline apply delegated to router provisioner");
    } else {
        match apply_nftables_baseline() {
            Ok(_) => println!("[firewall] nftables baseline loaded"),
            Err(error) => eprintln!("[firewall] baseline not loaded: {error}"),
        }
    }

    loop {
        println!(
            "[firewall] health nft={} baseline_loaded={}",
            command_available("nft"),
            baseline_loaded()
        );
        thread::sleep(Duration::from_secs(60));
    }
}

fn apply_nftables_baseline() -> Result<(), String> {
    if dry_run() {
        return Ok(());
    }
    if !command_available("nft") {
        return Err("nft not installed".to_owned());
    }

    let path = std::env::var("AEGISOS_NFTABLES_CONFIG")
        .unwrap_or_else(|_| DEFAULT_RULESET_PATH.to_owned());
    let script = std::fs::read_to_string(&path).unwrap_or_else(|_| default_nft_script());
    let script = script
        .lines()
        .filter(|line| !line.trim_start().starts_with("flush table"))
        .collect::<Vec<_>>()
        .join("\n");

    delete_table("inet", "aegis");
    delete_table("ip", "aegis_nat");
    run_nft_script(&script)
}

fn delete_table(family: &str, name: &str) {
    let _ = Command::new("nft")
        .args(["delete", "table", family, name])
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .status();
}

fn run_nft_script(script: &str) -> Result<(), String> {
    let mut child = Command::new("nft")
        .arg("-f")
        .arg("-")
        .stdin(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .map_err(|error| error.to_string())?;
    child
        .stdin
        .as_mut()
        .ok_or("nft stdin unavailable")?
        .write_all(script.as_bytes())
        .map_err(|error| error.to_string())?;
    let output = child.wait_with_output().map_err(|error| error.to_string())?;
    if output.status.success() {
        Ok(())
    } else {
        Err(String::from_utf8_lossy(&output.stderr).trim().to_owned())
    }
}

fn default_nft_script() -> String {
    r#"
table inet aegis {
  chain input {
    type filter hook input priority 0; policy drop;
    iifname "lo" accept
    ct state established,related accept
    ip protocol icmp accept
    counter log prefix "AEGIS-DROP-IN " drop
  }

  chain forward {
    type filter hook forward priority 0; policy drop;
    ct state established,related accept
    counter log prefix "AEGIS-DROP-FWD " drop
  }

  chain output {
    type filter hook output priority 0; policy accept;
  }
}
"#
    .to_owned()
}

fn baseline_loaded() -> bool {
    Command::new("nft")
        .args(["list", "ruleset"])
        .output()
        .map(|output| {
            output.status.success()
                && String::from_utf8_lossy(&output.stdout).contains("table inet aegis")
        })
        .unwrap_or(false)
}

fn command_available(command: &str) -> bool {
    Command::new("sh")
        .arg("-c")
        .arg(format!("command -v {command} >/dev/null 2>&1"))
        .status()
        .map(|status| status.success())
        .unwrap_or(false)
}

fn dry_run() -> bool {
    std::env::var("AEGISOS_ROUTER_DRY_RUN")
        .map(|value| value != "0")
        .unwrap_or(false)
}
