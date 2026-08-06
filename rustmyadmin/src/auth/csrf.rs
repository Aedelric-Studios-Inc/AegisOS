//! Per-session CSRF token generation and validation.

use std::collections::HashMap;
use std::io::Read;
use std::sync::{Mutex, OnceLock};
use std::time::{SystemTime, UNIX_EPOCH};

const CSRF_TTL_SECONDS: u64 = 12 * 60 * 60;

#[derive(Clone)]
struct CsrfEntry {
    token: String,
    expires: u64,
}

static CSRF_TOKENS: OnceLock<Mutex<HashMap<String, CsrfEntry>>> = OnceLock::new();

fn tokens() -> &'static Mutex<HashMap<String, CsrfEntry>> {
    CSRF_TOKENS.get_or_init(|| Mutex::new(HashMap::new()))
}

pub fn get_or_generate(session_id: &str) -> Result<String, String> {
    let now = unix_now();
    {
        let mut map = tokens()
            .lock()
            .map_err(|error| format!("CSRF store lock failed: {error}"))?;
        map.retain(|_, entry| entry.expires > now);
        if let Some(entry) = map.get(session_id) {
            return Ok(entry.token.clone());
        }
    }
    generate(session_id)
}

pub fn generate(session_id: &str) -> Result<String, String> {
    let token = generate_token()?;
    let expires = unix_now().saturating_add(CSRF_TTL_SECONDS);
    let mut map = tokens()
        .lock()
        .map_err(|error| format!("CSRF store lock failed: {error}"))?;
    map.insert(
        session_id.to_owned(),
        CsrfEntry {
            token: token.clone(),
            expires,
        },
    );
    Ok(token)
}

pub fn validate(session_id: &str, candidate: &str) -> bool {
    if candidate.is_empty() {
        return false;
    }
    if let Ok(map) = tokens().lock() {
        if let Some(entry) = map.get(session_id) {
            return unix_now() < entry.expires
                && constant_time_eq(entry.token.as_bytes(), candidate.as_bytes());
        }
    }
    false
}

pub fn revoke(session_id: &str) {
    if let Ok(mut map) = tokens().lock() {
        map.remove(session_id);
    }
}

fn unix_now() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs()
}

fn generate_token() -> Result<String, String> {
    let mut bytes = [0_u8; 32];
    let mut random = std::fs::File::open("/dev/urandom")
        .map_err(|error| format!("open /dev/urandom for CSRF token: {error}"))?;
    random
        .read_exact(&mut bytes)
        .map_err(|error| format!("read /dev/urandom for CSRF token: {error}"))?;
    Ok(hex(&bytes))
}

fn hex(bytes: &[u8]) -> String {
    bytes.iter().map(|byte| format!("{byte:02x}")).collect()
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
