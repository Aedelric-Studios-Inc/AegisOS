//! Access audit logging for RustMyAdmin.

use std::net::IpAddr;

pub fn record_access(ip: IpAddr, username: &str, method: &str, path: &str, status: u16) {
    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs();
    eprintln!(
        "[rustmyadmin] {timestamp} actor={username} ip={ip} method={method} path={path} status={status}"
    );
}
