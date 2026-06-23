//! Session management for rustmyadmin.

use std::collections::HashMap;
use std::io::Read;
use std::sync::{Mutex, OnceLock};
use std::time::{Duration, SystemTime, UNIX_EPOCH};

const SESSION_TTL: Duration = Duration::from_secs(3600);

struct Session {
    username: String,
    expires:  u64,
}

static SESSIONS: OnceLock<Mutex<HashMap<String, Session>>> = OnceLock::new();

fn sessions_map() -> &'static Mutex<HashMap<String, Session>> {
    SESSIONS.get_or_init(|| Mutex::new(HashMap::new()))
}

/// Create a new session for the given username; returns the session token.
pub fn create(username: &str) -> String {
    let token = generate_token();
    let expires = unix_now() + SESSION_TTL.as_secs();
    if let Ok(mut map) = sessions_map().lock() {
        let now = unix_now();
        map.retain(|_, s| s.expires > now);
        map.insert(token.clone(), Session { username: username.to_owned(), expires });
    }
    token
}

/// Look up a valid session; returns `Some(username)` if active.
pub fn lookup(token: &str) -> Option<String> {
    let now = unix_now();
    let mut map = sessions_map().lock().ok()?;
    let session = map.get_mut(token)?;
    if session.expires < now {
        map.remove(token);
        return None;
    }
    session.expires = now + SESSION_TTL.as_secs();
    Some(session.username.clone())
}

/// Revoke a session.
pub fn revoke(token: &str) {
    if let Ok(mut map) = sessions_map().lock() {
        map.remove(token);
    }
}

fn unix_now() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs()
}

fn generate_token() -> String {
    let mut b = [0u8; 32];
    let mut f = std::fs::File::open("/dev/urandom")
        .expect("/dev/urandom is required for rustmyadmin session tokens");
    f.read_exact(&mut b)
        .expect("failed to read rustmyadmin session entropy");
    hex(&b)
}

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{:02x}", x)).collect()
}
