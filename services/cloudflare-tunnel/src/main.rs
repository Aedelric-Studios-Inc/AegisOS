//! AegisOS Cloudflare Tunnel client.

use std::net::TcpStream;
use std::io::{Read, Write};
use std::time::Duration;

const CLOUDFLARE_EDGE: &str = "region1.v2.argotunnel.com:7844";
const CONFIG_PATH: &str = "/etc/cloudflare-tunnel/config.yml";
const RECONNECT_DELAY: Duration = Duration::from_secs(10);

fn main() {
    println!("[cloudflare-tunnel] service started");
    let token = load_token().unwrap_or_else(|e| {
        eprintln!("[cloudflare-tunnel] config error: {}", e);
        String::new()
    });

    if token.is_empty() {
        eprintln!("[cloudflare-tunnel] no tunnel token configured; idling");
        idle_loop();
    }

    loop {
        println!("[cloudflare-tunnel] connecting to {}", CLOUDFLARE_EDGE);
        match TcpStream::connect(CLOUDFLARE_EDGE) {
            Ok(mut stream) => {
                stream.set_read_timeout(Some(Duration::from_secs(30))).ok();
                println!("[cloudflare-tunnel] connected");
                if let Err(e) = run_tunnel(&mut stream, &token) {
                    eprintln!("[cloudflare-tunnel] tunnel error: {}", e);
                }
            }
            Err(e) => eprintln!("[cloudflare-tunnel] connect failed: {}", e),
        }
        println!("[cloudflare-tunnel] reconnecting in {}s", RECONNECT_DELAY.as_secs());
        std::thread::sleep(RECONNECT_DELAY);
    }
}

fn load_token() -> Result<String, String> {
    let content = std::fs::read_to_string(CONFIG_PATH)
        .map_err(|e| format!("{}: {}", CONFIG_PATH, e))?;
    for line in content.lines() {
        let line = line.trim();
        if let Some(rest) = line.strip_prefix("token:") {
            return Ok(rest.trim().trim_matches('"').to_owned());
        }
    }
    Err("no 'token:' field in config".to_owned())
}

/// Run the tunnel protocol over `stream`.
///
/// The Cloudflare Tunnel (formerly Argo Tunnel / cloudflared) uses the QUIC
/// or HTTP/2 CONNECT protocol over TLS.  This implementation sends an HTTP/1.1
/// CONNECT upgrade as a placeholder; a full implementation would use TLS + QUIC.
fn run_tunnel(stream: &mut TcpStream, token: &str) -> Result<(), String> {
    // Send a minimal HTTP/1.1 CONNECT request with the tunnel token.
    let req = format!(
        "CONNECT tunnel.cloudflare.com HTTP/1.1\r\n\
         Host: tunnel.cloudflare.com\r\n\
         CF-Access-Token: {}\r\n\
         \r\n",
        token
    );
    stream.write_all(req.as_bytes()).map_err(|e| e.to_string())?;

    // Read the server response.
    let mut resp_buf = [0u8; 256];
    let n = stream.read(&mut resp_buf).map_err(|e| e.to_string())?;
    let resp = String::from_utf8_lossy(&resp_buf[..n]);
    if !resp.starts_with("HTTP/1.1 200") {
        return Err(format!("unexpected response: {}", resp.lines().next().unwrap_or("")));
    }

    println!("[cloudflare-tunnel] tunnel established");

    // Keep-alive loop: forward bytes between tunnel and local services.
    let mut buf = [0u8; 4096];
    let mut total_bytes: u64 = 0;
    loop {
        match stream.read(&mut buf) {
            Ok(0) => return Err("server closed connection".to_owned()),
            Ok(n) => {
                total_bytes += n as u64;
                // TODO: parse Cloudflare Tunnel protocol frames and forward to
                // the appropriate local service.  The full protocol uses QUIC
                // or HTTP/2 multiplexing over TLS; a complete implementation
                // requires those transport libraries.  For now we drain the
                // socket so the connection stays alive and log traffic volume.
                if total_bytes % (64 * 1024) < n as u64 {
                    eprintln!(
                        "[cloudflare-tunnel] proxied {} KB (full protocol not yet implemented)",
                        total_bytes / 1024
                    );
                }
            }
            Err(e) => return Err(e.to_string()),
        }
    }
}

fn idle_loop() -> ! {
    loop { std::thread::sleep(RECONNECT_DELAY); }
}
