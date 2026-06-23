//! Authentication for aegisd.
//!
//! The daemon socket is protected by filesystem permissions (0660).
//! Production requests are also fail-closed unless AEGISD_TOKEN is configured.
//! For local bring-up only, set AEGISD_DEV_TRUST_LOCAL=1.

use std::env;

fn constant_time_eq(a: &str, b: &str) -> bool {
    let ab = a.as_bytes();
    let bb = b.as_bytes();
    if ab.len() != bb.len() {
        return false;
    }
    let mut diff = 0u8;
    for (&x, &y) in ab.iter().zip(bb.iter()) {
        diff |= x ^ y;
    }
    diff == 0
}

/// Validate an optional bearer token from the request.
pub fn validate_token(token: Option<&str>) -> bool {
    match env::var("AEGISD_TOKEN") {
        Ok(expected) => token
            .map(|t| {
                let candidate = t.trim().strip_prefix("Bearer ").unwrap_or(t.trim()).trim();
                constant_time_eq(candidate, expected.trim())
            })
            .unwrap_or(false),
        Err(_) => env::var("AEGISD_DEV_TRUST_LOCAL")
            .map(|v| v == "1" || v.eq_ignore_ascii_case("true"))
            .unwrap_or(false),
    }
}
