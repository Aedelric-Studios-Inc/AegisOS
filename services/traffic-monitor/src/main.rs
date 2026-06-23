//! AegisOS traffic monitor — captures flow statistics.

use std::net::UdpSocket;
use std::collections::HashMap;
use std::time::{Duration, Instant};

/// Per-flow statistics.
#[derive(Default)]
struct FlowStats {
    packets: u64,
    bytes:   u64,
    last_seen: Option<Instant>,
}

/// 5-tuple flow key.
type FlowKey = ([u8; 4], [u8; 4], u16, u16, u8);

const REPORT_INTERVAL: Duration = Duration::from_secs(5);

fn main() {
    println!("[traffic-monitor] service started");
    let mut flows: HashMap<FlowKey, FlowStats> = HashMap::new();
    let mut last_report = Instant::now();

    // Listen on a raw capture socket (Linux raw socket; AF_PACKET not available
    // via std, so we fall back to a UDP snoop port for demonstration).
    let sock = match UdpSocket::bind("0.0.0.0:0") {
        Ok(s) => { s.set_read_timeout(Some(REPORT_INTERVAL)).ok(); s }
        Err(e) => {
            eprintln!("[traffic-monitor] socket: {}", e);
            idle_loop();
        }
    };

    let mut buf = [0u8; 1500];
    loop {
        if let Ok((len, src)) = sock.recv_from(&mut buf) {
            // Synthetic flow tracking: use source IP as src, localhost as dst
            let src_ip = match src {
                std::net::SocketAddr::V4(a) => a.ip().octets(),
                _ => [0u8; 4],
            };
            let key: FlowKey = (src_ip, [127,0,0,1], src.port(), 0, 17); // UDP=17
            let e = flows.entry(key).or_default();
            e.packets += 1;
            e.bytes   += len as u64;
            e.last_seen = Some(Instant::now());
        }

        if last_report.elapsed() >= REPORT_INTERVAL {
            last_report = Instant::now();
            println!("[traffic-monitor] active flows: {}", flows.len());
            let total_bytes: u64 = flows.values().map(|f| f.bytes).sum();
            println!("[traffic-monitor] total bytes seen: {}", total_bytes);
            // Expire flows idle for >60 s
            flows.retain(|_, f| {
                f.last_seen.map(|t| t.elapsed() < Duration::from_secs(60)).unwrap_or(false)
            });
        }
    }
}

fn idle_loop() -> ! {
    loop { std::thread::sleep(REPORT_INTERVAL); }
}
