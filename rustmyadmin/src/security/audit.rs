//! Access audit logging for rustmyadmin.

use std::net::IpAddr;

/// Record that `ip` performed `method` on `path`.
pub fn record_access(ip: IpAddr, method: &str, path: &str) {
    let ts = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs();
    eprintln!("[rustmyadmin] {} {} {} {}", ts, ip, method, path);
}
