//! Persistent RustMyAdmin data models.

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ConfigEntry {
    pub key: String,
    pub value: String,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct UserRecord {
    pub id: u32,
    pub username: String,
    pub password_hash: String,
    pub role: String,
}
