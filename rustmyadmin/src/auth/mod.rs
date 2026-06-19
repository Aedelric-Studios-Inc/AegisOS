//! Authentication for rustmyadmin.

pub mod csrf;
pub mod password;
pub mod roles;
pub mod sessions;

use std::net::IpAddr;

/// Check whether an incoming request is authorized.
pub fn is_authorized(authorization: Option<&str>, peer_ip: IpAddr) -> bool {
    match std::env::var("RUSTMYADMIN_TOKEN") {
        Ok(expected) => {
            let token = authorization.unwrap_or("").trim();
            // Accept "******" or bare token.
            let candidate = token.strip_prefix("Bearer ").unwrap_or(token).trim();
            candidate == expected
        }
        Err(_) => {
            // No token configured: trust loopback connections only.
            peer_ip.is_loopback()
        }
    }
}
