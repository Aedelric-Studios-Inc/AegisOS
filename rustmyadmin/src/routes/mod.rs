//! Route dispatch for rustmyadmin.

pub mod auth;
pub mod backups;
pub mod containers;
pub mod dashboard;
pub mod firewall;
pub mod logs;
pub mod router;
pub mod services;
pub mod system;
pub mod updates;
pub mod users;
pub mod vpn;

/// Dispatch a request to the appropriate route handler.
/// Returns (status_code, content_type, body).
pub fn dispatch(method: &str, path: &str) -> (u16, &'static str, String) {
    // Strip query string.
    let path = path.split('?').next().unwrap_or(path);

    match (method, path) {
        ("GET", "/" | "/dashboard")        => (200, "text/html", dashboard::index()),
        ("GET", "/services")               => (200, "text/html", services::index()),
        ("GET", "/router")                => (200, "text/html", router::index()),
        ("GET", "/firewall")               => (200, "text/html", firewall::index()),
        ("GET", "/vpn")                    => (200, "text/html", vpn::index()),
        ("GET", "/containers")             => (200, "text/html", containers::index()),
        ("GET", "/backups")                => (200, "text/html", backups::index()),
        ("GET", "/updates")                => (200, "text/html", updates::index()),
        ("GET", "/logs")                   => (200, "text/html", logs::index()),
        ("GET", "/users")                  => (200, "text/html", users::index()),
        ("GET", "/system")                 => (200, "text/html", system::index()),
        ("GET", "/api/health")             => (200, "application/json", system::api_health()),
        ("GET", "/api/services")           => (200, "application/json", services::api_list()),
        ("GET", "/api/router")             => (200, "application/json", router::api_status()),
        ("GET", "/api/firewall")           => (200, "application/json", firewall::api_list()),
        ("GET", "/api/vpn")                => (200, "application/json", vpn::api_list()),
        ("GET", "/api/containers")         => (200, "application/json", containers::api_list()),
        ("GET", "/api/backups")            => (200, "application/json", backups::api_list()),
        ("GET", "/api/updates")            => (200, "application/json", updates::api_list()),
        ("GET", "/api/logs")               => (200, "application/json", logs::api_list()),
        _ => (404, "text/plain", "Not found".to_owned()),
    }
}
