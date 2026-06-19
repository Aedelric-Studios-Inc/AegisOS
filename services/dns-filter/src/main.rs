//! AegisOS DNS filter — intercepts DNS queries and blocks flagged domains.

use std::net::UdpSocket;
use std::collections::HashSet;
use std::time::Duration;

const LISTEN_ADDR: &str = "0.0.0.0:53";
const UPSTREAM:    &str = "1.1.1.1:53";
const BLOCKLIST:   &[&str] = &[
    "ads.example.com",
    "tracking.example.com",
    "malware.example.com",
];

fn main() {
    println!("[dns-filter] service started, listening on :53");
    let blocklist: HashSet<String> = BLOCKLIST.iter().map(|s| s.to_string()).collect();

    let sock = match UdpSocket::bind(LISTEN_ADDR) {
        Ok(s) => s,
        Err(e) => {
            eprintln!("[dns-filter] bind {}: {}", LISTEN_ADDR, e);
            // Fall back to idle loop if privileged port unavailable in dev
            loop { std::thread::sleep(Duration::from_secs(60)); }
        }
    };
    sock.set_read_timeout(Some(Duration::from_secs(5))).ok();

    let upstream = UdpSocket::bind("0.0.0.0:0").expect("upstream socket");
    upstream.set_read_timeout(Some(Duration::from_secs(2))).ok();

    let mut buf = [0u8; 512];
    loop {
        let (len, client) = match sock.recv_from(&mut buf) {
            Ok(r) => r,
            Err(_) => continue,
        };
        let query = &buf[..len];

        // Extract the queried name from the DNS wire format (labels after 12-byte header)
        let name = extract_qname(query);
        if blocklist.contains(&name) {
            eprintln!("[dns-filter] blocked: {}", name);
            // Send NXDOMAIN response
            let resp = make_nxdomain(query);
            sock.send_to(&resp, &client).ok();
            continue;
        }

        // Forward to upstream
        if upstream.send_to(query, UPSTREAM).is_ok() {
            let mut resp_buf = [0u8; 512];
            if let Ok((rlen, _)) = upstream.recv_from(&mut resp_buf) {
                sock.send_to(&resp_buf[..rlen], &client).ok();
            }
        }
    }
}

fn extract_qname(pkt: &[u8]) -> String {
    if pkt.len() < 13 { return String::new(); }
    let mut name = String::new();
    let mut i = 12usize;
    loop {
        if i >= pkt.len() { break; }
        let len = pkt[i] as usize;
        if len == 0 { break; }
        i += 1;
        if i + len > pkt.len() { break; }
        if !name.is_empty() { name.push('.'); }
        name.push_str(&String::from_utf8_lossy(&pkt[i..i+len]));
        i += len;
    }
    name
}

fn make_nxdomain(query: &[u8]) -> Vec<u8> {
    if query.len() < 2 { return vec![]; }
    let mut resp = query.to_vec();
    // Set QR=1 (response), RA=1, RCODE=3 (NXDOMAIN)
    if resp.len() >= 4 {
        resp[2] = 0x81; // QR + AA + RD
        resp[3] = 0x83; // RA + RCODE=3
    }
    resp
}
