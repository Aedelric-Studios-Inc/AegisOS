//! AegisOS LAN device discovery using the live kernel ARP table.

use std::collections::HashMap;
use std::env;
use std::fs;
use std::thread;
use std::time::{Duration, Instant};

const SCAN_INTERVAL: Duration = Duration::from_secs(15);

#[derive(Clone)]
struct Device {
    mac: String,
    interface: String,
    last_seen: Instant,
}

fn main() {
    let lan_interface = env::var("AEGIS_LAN_INTERFACE").unwrap_or_default();
    println!(
        "[device-discovery] service started, source=/proc/net/arp interface={}",
        if lan_interface.is_empty() {
            "all"
        } else {
            &lan_interface
        }
    );
    let mut devices: HashMap<String, Device> = HashMap::new();

    loop {
        match read_arp_table(&lan_interface) {
            Ok(entries) => {
                for (ip, mac, interface) in entries {
                    let is_new = !devices.contains_key(&ip);
                    devices.insert(
                        ip.clone(),
                        Device {
                            mac: mac.clone(),
                            interface: interface.clone(),
                            last_seen: Instant::now(),
                        },
                    );
                    if is_new {
                        println!(
                            "[device-discovery] new device ip={} mac={} interface={}",
                            ip, mac, interface
                        );
                    }
                }
                devices.retain(|_, device| device.last_seen.elapsed() < Duration::from_secs(300));
                println!(
                    "[device-discovery] active devices={} details={}",
                    devices.len(),
                    devices
                        .iter()
                        .map(|(ip, device)| format!("{}@{}({})", ip, device.interface, device.mac))
                        .collect::<Vec<_>>()
                        .join(",")
                );
            }
            Err(error) => eprintln!("[device-discovery] read ARP table: {error}"),
        }
        thread::sleep(SCAN_INTERVAL);
    }
}

fn read_arp_table(interface_filter: &str) -> std::io::Result<Vec<(String, String, String)>> {
    let content = fs::read_to_string("/proc/net/arp")?;
    let mut entries = Vec::new();
    for line in content.lines().skip(1) {
        let fields = line.split_whitespace().collect::<Vec<_>>();
        if fields.len() < 6 {
            continue;
        }
        let ip = fields[0];
        let flags = fields[2];
        let mac = fields[3];
        let interface = fields[5];
        if flags == "0x0" || mac == "00:00:00:00:00:00" {
            continue;
        }
        if !interface_filter.is_empty() && interface != interface_filter {
            continue;
        }
        entries.push((ip.to_owned(), mac.to_owned(), interface.to_owned()));
    }
    Ok(entries)
}
