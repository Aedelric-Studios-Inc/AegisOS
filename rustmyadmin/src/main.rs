//! RustMyAdmin — authenticated AegisOS administration interface.

mod aegisd_client;
mod app;
mod auth;
mod db;
mod routes;
mod security;

use std::net::{IpAddr, TcpListener};
use std::time::Duration;

fn main() -> std::io::Result<()> {
    if let Err(error) = auth::initialize() {
        return Err(std::io::Error::new(
            std::io::ErrorKind::Other,
            format!("could not initialize RustMyAdmin authentication: {error}"),
        ));
    }

    let port = std::env::var("RUSTMYADMIN_PORT")
        .ok()
        .and_then(|value| value.parse::<u16>().ok())
        .unwrap_or(8443);
    let address = format!("0.0.0.0:{port}");
    let listener = TcpListener::bind(&address)?;

    println!("[rustmyadmin] listening on http://{address}");
    println!(
        "[rustmyadmin] authentication configured: {}",
        auth::authentication_configured()
    );

    for incoming in listener.incoming() {
        match incoming {
            Ok(mut stream) => {
                if let Err(error) = stream.set_read_timeout(Some(Duration::from_secs(10))) {
                    eprintln!("[rustmyadmin] could not set read timeout: {error}");
                    continue;
                }
                if let Err(error) = stream.set_write_timeout(Some(Duration::from_secs(10))) {
                    eprintln!("[rustmyadmin] could not set write timeout: {error}");
                    continue;
                }
                let peer_ip = stream
                    .peer_addr()
                    .map(|address| address.ip())
                    .unwrap_or(IpAddr::from([127, 0, 0, 1]));
                if let Err(error) = app::handle_connection(&mut stream, peer_ip) {
                    eprintln!("[rustmyadmin] request error: {error}");
                }
            }
            Err(error) => eprintln!("[rustmyadmin] accept error: {error}"),
        }
    }

    Ok(())
}
