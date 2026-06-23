//! Application request handling.

use std::io::{Read, Write};
use std::net::IpAddr;

use crate::auth;
use crate::routes;
use crate::security;

pub fn handle_connection(
    stream: &mut std::net::TcpStream,
    peer_ip: IpAddr,
) -> std::io::Result<()> {
    let mut buf = [0u8; 8192];
    let n = stream.read(&mut buf)?;
    if n == 0 { return Ok(()); }

    let raw = String::from_utf8_lossy(&buf[..n]);
    let first_line = raw.lines().next().unwrap_or_default();
    let mut parts  = first_line.split_whitespace();
    let method     = parts.next().unwrap_or("GET");
    let path       = parts.next().unwrap_or("/");

    // Collect headers.
    let authorization = raw
        .lines()
        .find_map(|l| l.strip_prefix("Authorization:"))
        .map(str::trim);

    let (status, content_type, body) = if !auth::is_authorized(authorization, peer_ip) {
        (
            401u16,
            "text/plain",
            "Unauthorized. Set RUSTMYADMIN_TOKEN and send it as Authorization: ******"
                .to_owned(),
        )
    } else {
        security::audit::record_access(peer_ip, method, path);
        let (code, ct, b) = routes::dispatch(method, path);
        (code, ct, b)
    };

    let response = format!(
        "HTTP/1.1 {} {}\r\nContent-Type: {}\r\nContent-Length: {}\r\n{}\r\nConnection: close\r\n\r\n{}",
        status,
        http_status_text(status),
        content_type,
        body.len(),
        security::headers::security_headers(),
        body
    );

    stream.write_all(response.as_bytes())?;
    stream.flush()
}

fn http_status_text(code: u16) -> &'static str {
    match code {
        200 => "OK",
        400 => "Bad Request",
        401 => "Unauthorized",
        403 => "Forbidden",
        404 => "Not Found",
        405 => "Method Not Allowed",
        500 => "Internal Server Error",
        _   => "Unknown",
    }
}
