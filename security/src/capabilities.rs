//! Capability-based access control tokens.

use core::sync::atomic::{AtomicU64, Ordering};

static NEXT_TOKEN: AtomicU64 = AtomicU64::new(1);

/// Rights flags for a capability.
pub mod rights {
    pub const READ: u64 = 1 << 0;
    pub const WRITE: u64 = 1 << 1;
    pub const EXECUTE: u64 = 1 << 2;
    pub const GRANT: u64 = 1 << 3;
    pub const REVOKE: u64 = 1 << 4;
    pub const ALL: u64 = READ | WRITE | EXECUTE | GRANT | REVOKE;
}

/// An unforgeable capability token.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Capability {
    pub token: u64,
    pub owner_tid: u32,
    pub target_tid: u32,
    pub rights: u64,
}

impl Capability {
    /// Create a new capability with specified rights.
    pub fn create(owner_tid: u32, target_tid: u32, rights: u64) -> Self {
        Self {
            token: NEXT_TOKEN.fetch_add(1, Ordering::Relaxed),
            owner_tid,
            target_tid,
            rights,
        }
    }

    /// Check whether this capability grants the requested right.
    pub fn has_right(&self, right: u64) -> bool {
        self.rights & right == right
    }

    /// Derive a narrower capability (rights must be a subset).
    pub fn derive(&self, new_rights: u64) -> Option<Self> {
        if new_rights & !self.rights != 0 {
            return None; // Cannot escalate rights
        }
        Some(Self::create(self.owner_tid, self.target_tid, new_rights))
    }
}
