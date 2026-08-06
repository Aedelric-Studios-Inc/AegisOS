//! Authentication, sessions, CSRF protection, and role resolution.

pub mod csrf;
pub mod password;
pub mod roles;
pub mod sessions;

use std::net::IpAddr;

use crate::db;
use db::models::UserRecord;
use roles::Role;

pub const SESSION_COOKIE: &str = "rustmyadmin_session";

#[derive(Debug, Clone)]
pub struct Principal {
    pub username: String,
    pub role: Role,
    pub session_id: Option<String>,
    pub via_bearer: bool,
}

pub fn initialize() -> Result<(), String> {
    db::pool::set("config.schema_version", "1")?;

    if db::pool::load_user("admin").is_some() {
        return Ok(());
    }

    let password_hash = if let Ok(hash) = std::env::var("RUSTMYADMIN_ADMIN_PASSWORD_HASH") {
        let hash = hash.trim();
        (!hash.is_empty()).then(|| hash.to_owned())
    } else if let Ok(password_value) = std::env::var("RUSTMYADMIN_ADMIN_PASSWORD") {
        let password_value = password_value.trim();
        if password_value.is_empty() {
            None
        } else {
            Some(password::hash(password_value)?)
        }
    } else {
        None
    };

    if let Some(password_hash) = password_hash {
        db::pool::save_user(&UserRecord {
            id: 1,
            username: "admin".to_owned(),
            password_hash,
            role: Role::Admin.as_str().to_owned(),
        })?;
    }

    Ok(())
}

pub fn authentication_configured() -> bool {
    db::pool::load_user("admin").is_some()
        || configured_token().is_some()
        || dev_loopback_enabled()
}

pub fn authenticate_login(username: &str, secret: &str) -> Option<Principal> {
    let username = username.trim();
    if let Some(user) = db::pool::load_user(username) {
        if password::verify(secret, &user.password_hash) {
            return Some(Principal {
                username: user.username.clone(),
                role: Role::parse(&user.role).unwrap_or(Role::ReadOnly),
                session_id: None,
                via_bearer: false,
            });
        }
    }

    if username == "admin" && token_matches(secret) {
        return Some(Principal {
            username: "admin".to_owned(),
            role: Role::Admin,
            session_id: None,
            via_bearer: false,
        });
    }

    None
}

pub fn authorize_request(
    authorization: Option<&str>,
    browser_session: Option<&str>,
    peer_ip: IpAddr,
) -> Option<Principal> {
    if let Some(header) = authorization {
        let candidate = header
            .trim()
            .strip_prefix("Bearer ")
            .unwrap_or(header.trim())
            .trim();
        if token_matches(candidate) {
            return Some(Principal {
                username: "admin".to_owned(),
                role: Role::Admin,
                session_id: None,
                via_bearer: true,
            });
        }
    }

    if let Some(session_id) = browser_session {
        if let Some(username) = sessions::lookup(session_id) {
            return Some(Principal {
                role: roles::resolve(&username),
                username,
                session_id: Some(session_id.to_owned()),
                via_bearer: false,
            });
        }
    }

    if dev_loopback_enabled() && peer_ip.is_loopback() {
        return Some(Principal {
            username: "dev-loopback".to_owned(),
            role: Role::Admin,
            session_id: None,
            via_bearer: false,
        });
    }

    None
}

fn configured_token() -> Option<String> {
    std::env::var("RUSTMYADMIN_TOKEN")
        .ok()
        .map(|token| token.trim().to_owned())
        .filter(|token| !token.is_empty())
}

fn token_matches(candidate: &str) -> bool {
    configured_token()
        .map(|expected| constant_time_eq(candidate.trim().as_bytes(), expected.as_bytes()))
        .unwrap_or(false)
}

fn dev_loopback_enabled() -> bool {
    std::env::var("RUSTMYADMIN_DEV_TRUST_LOOPBACK")
        .map(|value| value == "1" || value.eq_ignore_ascii_case("true"))
        .unwrap_or(false)
}

fn constant_time_eq(left: &[u8], right: &[u8]) -> bool {
    if left.len() != right.len() {
        return false;
    }
    let mut difference = 0_u8;
    for (left, right) in left.iter().zip(right.iter()) {
        difference |= left ^ right;
    }
    difference == 0
}
