//! Role-based access control for rustmyadmin.

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Role {
    Admin,
    ReadOnly,
    Operator,
}

impl Role {
    pub fn can_write(self) -> bool {
        matches!(self, Role::Admin | Role::Operator)
    }

    pub fn can_manage_users(self) -> bool {
        matches!(self, Role::Admin)
    }
}

/// Resolve the role for an authenticated user.
pub fn resolve(_username: &str) -> Role {
    // In a real deployment this would query a user DB.
    // Default: single admin account.
    Role::Admin
}
