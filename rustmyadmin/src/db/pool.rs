//! File-backed configuration and user store.

use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};
use std::sync::{Mutex, OnceLock};

use super::models::{ConfigEntry, UserRecord};

const SYSTEM_DB_PATH: &str = "/var/lib/rustmyadmin/config.db";
const LOCAL_DB_SUFFIX: &str = ".local/state/rustmyadmin/config.db";

static POOL: OnceLock<Mutex<HashMap<String, String>>> = OnceLock::new();

fn database_path() -> PathBuf {
    if let Some(path) = std::env::var_os("RUSTMYADMIN_DB_PATH") {
        return PathBuf::from(path);
    }

    if Path::new("/etc/aegisos.toml").exists() {
        return PathBuf::from(SYSTEM_DB_PATH);
    }

    std::env::var_os("HOME")
        .map(PathBuf::from)
        .map(|home| home.join(LOCAL_DB_SUFFIX))
        .unwrap_or_else(|| PathBuf::from(SYSTEM_DB_PATH))
}

fn pool() -> &'static Mutex<HashMap<String, String>> {
    POOL.get_or_init(|| Mutex::new(load_from_disk(&database_path()).unwrap_or_default()))
}

pub fn get(key: &str) -> Option<String> {
    pool().lock().ok()?.get(key).cloned()
}

pub fn set(key: &str, value: &str) -> Result<(), String> {
    validate_field(key, "key")?;
    validate_field(value, "value")?;
    let mut map = pool().lock().map_err(|error| error.to_string())?;
    let mut next = map.clone();
    next.insert(key.to_owned(), value.to_owned());
    persist(&database_path(), &next).map_err(|error| error.to_string())?;
    *map = next;
    Ok(())
}

pub fn load_user(username: &str) -> Option<UserRecord> {
    let prefix = user_prefix(username)?;
    Some(UserRecord {
        id: get(&format!("{prefix}id"))?.parse().ok()?,
        username: username.to_owned(),
        password_hash: get(&format!("{prefix}password_hash"))?,
        role: get(&format!("{prefix}role"))?,
    })
}

pub fn save_user(user: &UserRecord) -> Result<(), String> {
    validate_username(&user.username)?;
    validate_field(&user.password_hash, "password hash")?;
    validate_field(&user.role, "role")?;

    let prefix = user_prefix(&user.username).ok_or_else(|| "invalid username".to_owned())?;
    let mut map = pool().lock().map_err(|error| error.to_string())?;
    let mut next = map.clone();
    next.insert(format!("{prefix}id"), user.id.to_string());
    next.insert(
        format!("{prefix}password_hash"),
        user.password_hash.clone(),
    );
    next.insert(format!("{prefix}role"), user.role.clone());
    persist(&database_path(), &next).map_err(|error| error.to_string())?;
    *map = next;
    Ok(())
}

pub fn delete_user(username: &str) -> Result<bool, String> {
    let prefix = user_prefix(username).ok_or_else(|| "invalid username".to_owned())?;
    let mut map = pool().lock().map_err(|error| error.to_string())?;
    let mut next = map.clone();
    let before = next.len();
    next.retain(|key, _| !key.starts_with(&prefix));
    let removed = next.len() != before;
    if removed {
        persist(&database_path(), &next).map_err(|error| error.to_string())?;
        *map = next;
    }
    Ok(removed)
}

pub fn list_users() -> Vec<UserRecord> {
    let usernames = pool()
        .lock()
        .map(|map| {
            let mut names = map
                .keys()
                .filter_map(|key| {
                    let rest = key.strip_prefix("user.")?;
                    let (username, field) = rest.split_once('.')?;
                    (field == "id").then(|| username.to_owned())
                })
                .collect::<Vec<_>>();
            names.sort();
            names.dedup();
            names
        })
        .unwrap_or_default();

    usernames
        .into_iter()
        .filter_map(|username| load_user(&username))
        .collect()
}

pub fn next_user_id() -> u32 {
    list_users()
        .into_iter()
        .map(|user| user.id)
        .max()
        .unwrap_or(0)
        .saturating_add(1)
}

pub fn list_config_entries() -> Vec<ConfigEntry> {
    pool()
        .lock()
        .map(|map| {
            let mut entries = map
                .iter()
                .filter(|(key, _)| key.starts_with("config."))
                .map(|(key, value)| ConfigEntry {
                    key: key.clone(),
                    value: value.clone(),
                })
                .collect::<Vec<_>>();
            entries.sort_by(|left, right| left.key.cmp(&right.key));
            entries
        })
        .unwrap_or_default()
}

fn user_prefix(username: &str) -> Option<String> {
    validate_username(username).ok()?;
    Some(format!("user.{username}."))
}

fn validate_username(username: &str) -> Result<(), String> {
    if username.is_empty() || username.len() > 64 {
        return Err("username must contain 1 to 64 characters".to_owned());
    }
    if username
        .bytes()
        .all(|byte| byte.is_ascii_alphanumeric() || matches!(byte, b'-' | b'_'))
    {
        Ok(())
    } else {
        Err("username may only contain letters, numbers, '-' and '_'".to_owned())
    }
}

fn validate_field(value: &str, label: &str) -> Result<(), String> {
    if value.contains('\n') || value.contains('\r') {
        Err(format!("{label} may not contain a newline"))
    } else {
        Ok(())
    }
}

fn load_from_disk(path: &Path) -> Option<HashMap<String, String>> {
    let content = fs::read_to_string(path).ok()?;
    let mut map = HashMap::new();
    for line in content.lines() {
        if line.trim().is_empty() || line.trim_start().starts_with('#') {
            continue;
        }
        if let Some((key, value)) = line.split_once('=') {
            map.insert(key.to_owned(), value.to_owned());
        }
    }
    Some(map)
}

fn persist(path: &Path, map: &HashMap<String, String>) -> std::io::Result<()> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }

    let mut entries = map.iter().collect::<Vec<_>>();
    entries.sort_by(|(left, _), (right, _)| left.cmp(right));
    let content = entries
        .into_iter()
        .map(|(key, value)| format!("{key}={value}\n"))
        .collect::<String>();

    let temporary = path.with_extension("db.tmp");
    fs::write(&temporary, content)?;
    fs::rename(temporary, path)
}
