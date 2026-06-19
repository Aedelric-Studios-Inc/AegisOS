//! Authentication for aegisd.
//!
//! The daemon socket is protected by filesystem permissions (0660, root:aegis).
//! Callers already authenticated by the OS kernel when they connect.
//! This module validates optional per-request bearer tokens for future
//! multi-user support.

use std::env;

/// Validate an optional bearer token from the request.
///
/// If `AEGISD_TOKEN` is set in the environment, the caller must present it.
/// If the env var is absent, any local connection is trusted.
pub fn validate_token(token: Option<&str>) -> bool {
    match env::var("AEGISD_TOKEN") {
        Ok(expected) => token
            .map(|t| t.trim().trim_start_matches("Bearer ").trim() == expected)
            .unwrap_or(false),
        Err(_) => true, // no token configured → trust socket-level auth
    }
}
