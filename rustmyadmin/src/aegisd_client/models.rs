//! Data models returned by aegisd.

#[derive(Debug)]
pub struct HealthResponse {
    pub status:         String,
    pub uptime_seconds: u64,
}

#[derive(Debug)]
pub struct ServiceEntry {
    pub name:   String,
    pub active: bool,
}

#[derive(Debug)]
pub struct FirewallRule {
    pub id:       u32,
    pub chain:    String,
    pub protocol: String,
    pub src:      String,
    pub dst:      String,
    pub port:     Option<u16>,
    pub action:   String,
}
