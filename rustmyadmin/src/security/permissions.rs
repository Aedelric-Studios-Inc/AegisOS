//! Route permission checks for RustMyAdmin.

use crate::auth::roles::Role;

pub fn can_access_route(role: Role, method: &str, path: &str) -> bool {
    if matches!(method, "GET" | "HEAD") {
        return true;
    }

    if path.starts_with("/users") || path.starts_with("/api/users") {
        return role.can_manage_users();
    }

    role.can_write()
}
