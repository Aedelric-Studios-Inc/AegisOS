//! In-memory browser session management for RustMyAdmin.

use std::collections::HashMap;
use std::io::Read;
use std::sync::{Mutex, OnceLock};
use std::time::{Duration, SystemTime, UNIX_EPOCH};

const SESSION_TTL: Duration = Duration::from_secs(12 * 60 * 60);

struct Session {
    username: String,
    expires: u64,
}

static SESSIONS: OnceLock<Mutex<HashMap<String, Session>>> = OnceLock::new();

fn sessions_map() -> &'static Mutex<HashMap<String, Session>> {
    SESSIONS.get_or_init(|| Mutex::new(HashMap::new()))
}

pub fn create(username: &str) -> Result<String, String> {
    let token = generate_token()?;
    let expires = unix_now().saturating_add(SESSION_TTL.as_secs());
    let mut map = sessions_map()
        .lock()
        .map_err(|error| format!("session store lock failed: {error}"))?;
    let now = unix_now();
    map.retain(|_, session| session.expires > now);
    map.insert(
        token.clone(),
        Session {
            username: username.to_owned(),
            expires,
        },
    );
    Ok(token)
}

pub fn lookup(token: &str) -> Option<String> {
    let now = unix_now();
    let mut map = sessions_map().lock().ok()?;
    let session = map.get_mut(token)?;
    if session.expires <= now {
        map.remove(token);
        return None;
    }
    session.expires = now.saturating_add(SESSION_TTL.as_secs());
    Some(session.username.clone())
}

pub fn revoke(token: &str) {
    if let Ok(mut map) = sessions_map().lock() {
        map.remove(token);
    }
}

pub fn revoke_user(username: &str) {
    if let Ok(mut map) = sessions_map().lock() {
        map.retain(|_, session| session.username != username);
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
        .map_err(|error| format!("open /dev/urandom for session token: {error}"))?;
    random
        .read_exact(&mut bytes)
        .map_err(|error| format!("read /dev/urandom for session token: {error}"))?;
    Ok(hex(&bytes))
}

fn hex(bytes: &[u8]) -> String {
    bytes.iter().map(|byte| format!("{byte:02x}")).collect()
}
