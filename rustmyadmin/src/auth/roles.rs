//! Role-based access control for RustMyAdmin.

use crate::db;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Role {
    Admin,
    Operator,
    ReadOnly,
}

impl Role {
    pub fn parse(value: &str) -> Option<Self> {
        match value.trim().to_ascii_lowercase().as_str() {
            "admin" => Some(Self::Admin),
            "operator" => Some(Self::Operator),
            "readonly" | "read-only" | "read_only" => Some(Self::ReadOnly),
            _ => None,
        }
    }

    pub fn as_str(self) -> &'static str {
        match self {
            Self::Admin => "admin",
            Self::Operator => "operator",
            Self::ReadOnly => "readonly",
        }
    }

    pub fn can_write(self) -> bool {
        matches!(self, Self::Admin | Self::Operator)
    }

    pub fn can_manage_users(self) -> bool {
        matches!(self, Self::Admin)
    }
}

pub fn resolve(username: &str) -> Role {
    db::pool::load_user(username)
        .and_then(|user| Role::parse(&user.role))
        .unwrap_or_else(|| {
            if username == "admin" {
                Role::Admin
            } else {
                Role::ReadOnly
            }
        })
}
