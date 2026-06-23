//! Authentication for rustmyadmin.

pub mod csrf;
pub mod password;
pub mod roles;
pub mod sessions;

use std::net::IpAddr;

fn constant_time_eq(a: &str, b: &str) -> bool {
    let ab = a.as_bytes();
    let bb = b.as_bytes();
    if ab.len() != bb.len() { return false; }
    let mut diff = 0u8;
    for (&x, &y) in ab.iter().zip(bb.iter()) { diff |= x ^ y; }
    diff == 0
}

/// Check whether an incoming request is authorised.
///
/// Production is fail-closed: RUSTMYADMIN_TOKEN must be set.
/// For local bring-up only, set RUSTMYADMIN_DEV_TRUST_LOOPBACK=1 to allow
/// loopback requests without a bearer token.
pub fn is_authorized(authorization: Option<&str>, peer_ip: IpAddr) -> bool {
    if let Ok(expected) = std::env::var("RUSTMYADMIN_TOKEN") {
        let token = authorization.unwrap_or("").trim();
        let candidate = token.strip_prefix("Bearer ").unwrap_or(token).trim();
        return constant_time_eq(candidate, expected.trim());
    }

    std::env::var("RUSTMYADMIN_DEV_TRUST_LOOPBACK")
        .map(|v| v == "1" || v.eq_ignore_ascii_case("true"))
        .unwrap_or(false)
        && peer_ip.is_loopback()
}
