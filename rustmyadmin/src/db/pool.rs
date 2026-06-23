//! Database connection pool (file-backed key-value store).

use std::collections::HashMap;
use std::sync::{Mutex, OnceLock};
use std::fs;

const DB_PATH: &str = "/var/lib/rustmyadmin/config.db";

static POOL: OnceLock<Mutex<HashMap<String, String>>> = OnceLock::new();

fn pool() -> &'static Mutex<HashMap<String, String>> {
    POOL.get_or_init(|| {
        let map = load_from_disk().unwrap_or_default();
        Mutex::new(map)
    })
}

pub fn get(key: &str) -> Option<String> {
    pool().lock().ok()?.get(key).cloned()
}

pub fn set(key: &str, value: &str) -> Result<(), String> {
    let mut map = pool().lock().map_err(|e| e.to_string())?;
    map.insert(key.to_owned(), value.to_owned());
    persist(&map).map_err(|e| e.to_string())
}

fn load_from_disk() -> Option<HashMap<String, String>> {
    let content = fs::read_to_string(DB_PATH).ok()?;
    let mut map = HashMap::new();
    for line in content.lines() {
        if let Some((k, v)) = line.split_once('=') {
            map.insert(k.to_owned(), v.to_owned());
        }
    }
    Some(map)
}

fn persist(map: &HashMap<String, String>) -> std::io::Result<()> {
    if let Some(parent) = std::path::Path::new(DB_PATH).parent() {
        fs::create_dir_all(parent)?;
    }
    let content: String = map.iter()
        .map(|(k, v)| format!("{}={}\n", k, v))
        .collect();
    fs::write(DB_PATH, content)
}
