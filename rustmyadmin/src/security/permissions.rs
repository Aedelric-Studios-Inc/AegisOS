//! Permission checks for rustmyadmin routes.

use crate::auth::roles::Role;

pub fn can_access_route(role: Role, path: &str) -> bool {
    // Read-only users cannot access write-capable routes.
    if role == Role::ReadOnly {
        let write_routes = ["/users/add", "/users/remove", "/services/start", "/services/stop"];
        return !write_routes.iter().any(|r| path.starts_with(r));
    }
    true
}
