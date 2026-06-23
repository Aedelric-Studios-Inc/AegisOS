//! AegisOS LAN device discovery (mDNS / ARP scan).

use std::net::{UdpSocket, Ipv4Addr, SocketAddr};
use std::collections::HashMap;
use std::time::{Duration, Instant};

const SCAN_INTERVAL: Duration = Duration::from_secs(30);

fn main() {
    println!("[device-discovery] service started");
    let mut devices: HashMap<[u8; 4], Instant> = HashMap::new();

    let sock = match UdpSocket::bind("0.0.0.0:5353") {
        Ok(s) => s,
        Err(e) => {
            eprintln!("[device-discovery] bind mDNS port: {}", e);
            idle_loop();
        }
    };
    sock.set_read_timeout(Some(SCAN_INTERVAL)).ok();

    // Join mDNS multicast group
    let multicast = Ipv4Addr::new(224, 0, 0, 251);
    sock.join_multicast_v4(&multicast, &Ipv4Addr::UNSPECIFIED).ok();

    let mut last_scan = Instant::now();

    loop {
        // Passive mDNS listener
        let mut buf = [0u8; 1500];
        if let Ok((len, src)) = sock.recv_from(&mut buf) {
            if let SocketAddr::V4(addr) = src {
                let ip = addr.ip().octets();
                if devices.insert(ip, Instant::now()).is_none() {
                    println!("[device-discovery] new device: {}", addr.ip());
                }
                let _ = len; // DNS name parsing would go here
            }
        }

        // Periodic ARP sweep announcement
        if last_scan.elapsed() >= SCAN_INTERVAL {
            last_scan = Instant::now();
            println!("[device-discovery] discovered {} devices on LAN", devices.len());
            // Prune devices not seen in 5 minutes
            devices.retain(|_, t| t.elapsed() < Duration::from_secs(300));
        }
    }
}

fn idle_loop() -> ! {
    loop { std::thread::sleep(SCAN_INTERVAL); }
}
