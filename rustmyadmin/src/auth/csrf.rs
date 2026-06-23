//! CSRF token generation and validation.

use std::collections::HashMap;
use std::io::Read;
use std::sync::{Mutex, OnceLock};

#[derive(Clone)]
struct CsrfEntry {
    token: String,
    expires: u64,
}

static CSRF_TOKENS: OnceLock<Mutex<HashMap<String, CsrfEntry>>> = OnceLock::new();

fn tokens() -> &'static Mutex<HashMap<String, CsrfEntry>> {
    CSRF_TOKENS.get_or_init(|| Mutex::new(HashMap::new()))
}

/// Generate a new CSRF token for a session.
pub fn generate(session_id: &str) -> String {
    let token = generate_token();
    let expires = unix_now() + 3600;
    if let Ok(mut map) = tokens().lock() {
        let now = unix_now();
        map.retain(|_, entry| entry.expires > now);
        map.insert(session_id.to_owned(), CsrfEntry { token: token.clone(), expires });
    }
    token
}

/// Validate a CSRF token using constant-time comparison.
pub fn validate(session_id: &str, token: &str) -> bool {
    if token.is_empty() { return false; }
    if let Ok(map) = tokens().lock() {
        if let Some(entry) = map.get(session_id) {
            return unix_now() < entry.expires && constant_time_eq(&entry.token, token);
        }
    }
    false
}

fn unix_now() -> u64 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs()
}

fn generate_token() -> String {
    let mut b = [0u8; 32];
    let mut f = std::fs::File::open("/dev/urandom")
        .expect("/dev/urandom is required for rustmyadmin CSRF tokens");
    f.read_exact(&mut b)
        .expect("failed to read rustmyadmin CSRF entropy");
    hex(&b)
}

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{:02x}", x)).collect()
}

fn constant_time_eq(a: &str, b: &str) -> bool {
    let ab = a.as_bytes();
    let bb = b.as_bytes();
    if ab.len() != bb.len() { return false; }
    let mut diff = 0u8;
    for (&x, &y) in ab.iter().zip(bb.iter()) { diff |= x ^ y; }
    diff == 0
}
