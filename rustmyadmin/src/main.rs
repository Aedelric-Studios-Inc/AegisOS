//! rustmyadmin — AegisOS admin web interface.
//!
//! Provides a secure, authentication-gated HTTP admin panel.
//! Listens on port 8443 by default (override with RUSTMYADMIN_PORT env var).
//!
//! Authentication: HTTP Basic auth or ****** via RUSTMYADMIN_TOKEN env var.
//! If RUSTMYADMIN_TOKEN is unset, loopback connections are trusted.

mod aegisd_client;
mod app;
mod auth;
mod db;
mod routes;
mod security;

use std::io::{Read, Write};
use std::net::{IpAddr, TcpListener};
use std::time::Duration;

fn main() -> std::io::Result<()> {
    let port: u16 = std::env::var("RUSTMYADMIN_PORT")
        .ok()
        .and_then(|v| v.parse().ok())
        .unwrap_or(8443);

    let addr = format!("0.0.0.0:{}", port);
    let listener = TcpListener::bind(&addr)?;
    println!("[rustmyadmin] AegisOS Admin Panel listening on http://{}", addr);
    println!("[rustmyadmin] Set RUSTMYADMIN_TOKEN to require authentication");

    for incoming in listener.incoming() {
        match incoming {
            Ok(mut stream) => {
                stream.set_read_timeout(Some(Duration::from_secs(10))).ok();
                stream.set_write_timeout(Some(Duration::from_secs(10))).ok();
                let peer_ip = stream
                    .peer_addr()
                    .map(|a| a.ip())
                    .unwrap_or(IpAddr::from([127, 0, 0, 1]));
                if let Err(e) = app::handle_connection(&mut stream, peer_ip) {
                    eprintln!("[rustmyadmin] request error: {}", e);
                }
            }
            Err(e) => eprintln!("[rustmyadmin] accept: {}", e),
        }
    }
    Ok(())
}
