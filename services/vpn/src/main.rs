//! AegisOS VPN service (WireGuard-compatible tunnel).

use std::net::UdpSocket;
use std::time::Duration;
use std::fs;

const WG_DEFAULT_PORT: u16 = 51820;
const CONFIG_PATH: &str = "/etc/vpn/wg0.conf";

struct WgConfig {
    listen_port: u16,
    private_key: [u8; 32],
    peer_endpoint: Option<String>,
    peer_pubkey: [u8; 32],
    allowed_ips: Vec<String>,
}

fn main() {
    println!("[vpn] service started");
    let cfg = load_config().unwrap_or_else(|e| {
        eprintln!("[vpn] config error: {}, using defaults", e);
        WgConfig {
            listen_port:  WG_DEFAULT_PORT,
            private_key:  [0u8; 32],
            peer_endpoint: None,
            peer_pubkey:  [0u8; 32],
            allowed_ips:  Vec::new(),
        }
    });

    println!("[vpn] listening on UDP :{}", cfg.listen_port);
    let addr = format!("0.0.0.0:{}", cfg.listen_port);
    let sock = match UdpSocket::bind(&addr) {
        Ok(s) => s,
        Err(e) => {
            eprintln!("[vpn] bind {}: {}", addr, e);
            idle_loop();
        }
    };
    sock.set_read_timeout(Some(Duration::from_secs(5))).ok();

    // Initiate handshake with peer if configured
    if let Some(ref ep) = cfg.peer_endpoint {
        let handshake = build_initiation_message(&cfg.private_key, &cfg.peer_pubkey);
        match sock.send_to(&handshake, ep.as_str()) {
            Ok(_) => println!("[vpn] sent handshake initiation to {}", ep),
            Err(e) => eprintln!("[vpn] handshake to {}: {}", ep, e),
        }
    }

    let mut buf = [0u8; 1500];
    loop {
        match sock.recv_from(&mut buf) {
            Ok((len, src)) => handle_packet(&buf[..len], src, &cfg),
            Err(_) => {} // timeout or transient error
        }
    }
}

fn load_config() -> Result<WgConfig, String> {
    let content = fs::read_to_string(CONFIG_PATH)
        .map_err(|e| format!("{}: {}", CONFIG_PATH, e))?;
    let mut cfg = WgConfig {
        listen_port:  WG_DEFAULT_PORT,
        private_key:  [0u8; 32],
        peer_endpoint: None,
        peer_pubkey:  [0u8; 32],
        allowed_ips:  Vec::new(),
    };
    for line in content.lines() {
        let line = line.trim();
        if let Some(val) = strip_key(line, "ListenPort") {
            cfg.listen_port = val.parse().unwrap_or(WG_DEFAULT_PORT);
        }
        if let Some(val) = strip_key(line, "Endpoint") {
            cfg.peer_endpoint = Some(val.to_owned());
        }
        if let Some(val) = strip_key(line, "AllowedIPs") {
            cfg.allowed_ips.push(val.to_owned());
        }
    }
    Ok(cfg)
}

fn strip_key<'a>(line: &'a str, key: &str) -> Option<&'a str> {
    let prefix = format!("{} =", key);
    if line.starts_with(&prefix) {
        Some(line[prefix.len()..].trim())
    } else { None }
}

fn build_initiation_message(_private_key: &[u8; 32], _peer_pubkey: &[u8; 32]) -> Vec<u8> {
    // WireGuard message type 1 (initiation) — placeholder structure.
    // A full implementation requires Curve25519 DH and ChaCha20-Poly1305.
    let mut msg = vec![0u8; 148];
    msg[0] = 1; // message type: initiation
    msg
}

fn handle_packet(pkt: &[u8], src: std::net::SocketAddr, _cfg: &WgConfig) {
    if pkt.is_empty() { return; }
    match pkt[0] {
        1 => println!("[vpn] received handshake initiation from {}", src),
        2 => println!("[vpn] received handshake response from {}",   src),
        4 => { /* data packet — would decrypt and inject into tun device */ }
        t => eprintln!("[vpn] unknown message type {} from {}", t, src),
    }
}

fn idle_loop() -> ! {
    loop { std::thread::sleep(Duration::from_secs(10)); }
}
