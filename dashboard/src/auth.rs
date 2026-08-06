//! Dashboard authentication and browser session handling.

use std::collections::HashMap;
use std::io::Read;
use std::net::IpAddr;
use std::sync::{Mutex, OnceLock};
use std::time::{SystemTime, UNIX_EPOCH};

pub const SESSION_COOKIE: &str = "aegis_dashboard_session";
const SESSION_TTL_SECONDS: u64 = 12 * 60 * 60;

#[derive(Clone)]
struct BrowserSession {
    user: String,
    client_ip: IpAddr,
    expires_at: u64,
}

static SESSIONS: OnceLock<Mutex<HashMap<String, BrowserSession>>> = OnceLock::new();

pub struct Session {
    pub user: String,
}

fn sessions() -> &'static Mutex<HashMap<String, BrowserSession>> {
    SESSIONS.get_or_init(|| Mutex::new(HashMap::new()))
}

pub fn dashboard_token_configured() -> bool {
    std::env::var("AEGIS_DASHBOARD_TOKEN")
        .map(|value| !value.trim().is_empty())
        .unwrap_or(false)
}

pub fn authenticate(token: &str) -> Option<Session> {
    let expected = std::env::var("AEGIS_DASHBOARD_TOKEN").ok()?;
    if expected.trim().is_empty() {
        return None;
    }

    let candidate = token.trim().trim_start_matches("Bearer ").trim();
    if constant_time_eq(candidate.as_bytes(), expected.trim().as_bytes()) {
        Some(Session {
            user: "admin".to_owned(),
        })
    } else {
        None
    }
}

pub fn create_browser_session(user: &str, client_ip: IpAddr) -> std::io::Result<String> {
    let mut entropy = [0_u8; 32];
    std::fs::File::open("/dev/urandom")?.read_exact(&mut entropy)?;
    let session_id = hex(&entropy);
    let entry = BrowserSession {
        user: user.to_owned(),
        client_ip,
        expires_at: unix_now().saturating_add(SESSION_TTL_SECONDS),
    };

    let mut guard = sessions().lock().unwrap_or_else(|poisoned| poisoned.into_inner());
    purge_expired(&mut guard);
    guard.insert(session_id.clone(), entry);
    Ok(session_id)
}

pub fn validate_browser_session(session_id: Option<&str>, client_ip: IpAddr) -> bool {
    let Some(session_id) = session_id else {
        return false;
    };

    let mut guard = sessions().lock().unwrap_or_else(|poisoned| poisoned.into_inner());
    purge_expired(&mut guard);
    guard
        .get(session_id)
        .map(|session| !session.user.is_empty() && session.client_ip == client_ip)
        .unwrap_or(false)
}

pub fn revoke_browser_session(session_id: Option<&str>) {
    let Some(session_id) = session_id else {
        return;
    };

    let mut guard = sessions().lock().unwrap_or_else(|poisoned| poisoned.into_inner());
    guard.remove(session_id);
}

pub fn authorize_request(
    authorization: Option<&str>,
    browser_session: Option<&str>,
    client_ip: IpAddr,
) -> bool {
    if dashboard_token_configured() {
        authorization.and_then(authenticate).is_some()
            || validate_browser_session(browser_session, client_ip)
    } else {
        client_ip.is_loopback()
    }
}

fn purge_expired(sessions: &mut HashMap<String, BrowserSession>) {
    let now = unix_now();
    sessions.retain(|_, session| session.expires_at > now);
}

fn unix_now() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs()
}

fn hex(bytes: &[u8]) -> String {
    bytes.iter().map(|byte| format!("{byte:02x}")).collect()
}

fn constant_time_eq(a: &[u8], b: &[u8]) -> bool {
    if a.len() != b.len() {
        return false;
    }

    let mut diff = 0_u8;
    for (left, right) in a.iter().zip(b.iter()) {
        diff |= left ^ right;
    }
    diff == 0
}
