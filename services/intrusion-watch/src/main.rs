//! AegisOS intrusion detection service.

use std::net::UdpSocket;
use std::collections::HashMap;
use std::time::{Duration, Instant};

const SCAN_WINDOW: Duration = Duration::from_secs(60);
const PORT_SCAN_THRESHOLD: u32 = 15;   // distinct destination ports from one source
const FLOOD_THRESHOLD: u64 = 10_000;   // bytes per second

#[derive(Default)]
struct SourceStats {
    ports_seen:    std::collections::HashSet<u16>,
    bytes_in_window: u64,
    window_start:  Option<Instant>,
}

fn main() {
    println!("[intrusion-watch] service started");
    let mut stats: HashMap<[u8; 4], SourceStats> = HashMap::new();
    let mut last_cleanup = Instant::now();

    let sock = match UdpSocket::bind("0.0.0.0:0") {
        Ok(s) => { s.set_read_timeout(Some(Duration::from_secs(1))).ok(); s }
        Err(e) => {
            eprintln!("[intrusion-watch] socket: {}", e);
            idle_loop();
        }
    };

    let mut buf = [0u8; 1500];
    loop {
        if let Ok((len, src)) = sock.recv_from(&mut buf) {
            let src_ip = match src {
                std::net::SocketAddr::V4(a) => a.ip().octets(),
                _ => [0u8; 4],
            };
            let dst_port = src.port(); // using src port as proxy for dst port
            let e = stats.entry(src_ip).or_default();
            e.ports_seen.insert(dst_port);
            e.bytes_in_window += len as u64;
            if e.window_start.is_none() { e.window_start = Some(Instant::now()); }

            // Port scan detection
            if e.ports_seen.len() as u32 >= PORT_SCAN_THRESHOLD {
                eprintln!(
                    "[intrusion-watch] ALERT: port scan from {}.{}.{}.{} ({} ports)",
                    src_ip[0], src_ip[1], src_ip[2], src_ip[3], e.ports_seen.len()
                );
                e.ports_seen.clear();
            }

            // Flood detection
            let elapsed = e.window_start.map(|t| t.elapsed().as_secs().max(1)).unwrap_or(1);
            if e.bytes_in_window / elapsed >= FLOOD_THRESHOLD {
                eprintln!(
                    "[intrusion-watch] ALERT: traffic flood from {}.{}.{}.{} ({} B/s)",
                    src_ip[0], src_ip[1], src_ip[2], src_ip[3],
                    e.bytes_in_window / elapsed
                );
                e.bytes_in_window = 0;
                e.window_start = Some(Instant::now());
            }
        }

        // Periodic cleanup
        if last_cleanup.elapsed() >= SCAN_WINDOW {
            last_cleanup = Instant::now();
            stats.retain(|_, e| {
                e.window_start.map(|t| t.elapsed() < SCAN_WINDOW * 2).unwrap_or(false)
            });
        }
    }
}

fn idle_loop() -> ! {
    loop { std::thread::sleep(Duration::from_secs(5)); }
}
