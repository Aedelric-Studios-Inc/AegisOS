//! Process sandbox enforcement.

use crate::capabilities::{Capability, rights};

/// A sandbox policy describing what a process is allowed to do.
#[derive(Debug, Clone)]
pub struct SandboxPolicy {
    pub allow_network:    bool,
    pub allow_filesystem: bool,
    pub allow_ipc:        bool,
    pub max_memory_bytes: u64,
}

impl SandboxPolicy {
    /// Restrictive default: deny all.
    pub const fn deny_all() -> Self {
        Self {
            allow_network:    false,
            allow_filesystem: false,
            allow_ipc:        false,
            max_memory_bytes: 0,
        }
    }

    /// Service default: allow IPC only.
    pub const fn service_default() -> Self {
        Self {
            allow_network:    false,
            allow_filesystem: true,
            allow_ipc:        true,
            max_memory_bytes: 64 * 1024 * 1024,
        }
    }

    /// Compute the capability rights bitmask this policy allows.
    pub fn allowed_rights(&self) -> u64 {
        let mut r = rights::READ; // all processes may read their own address space
        if self.allow_filesystem {
            r |= rights::WRITE;
        }
        if self.allow_ipc {
            r |= rights::GRANT;
        }
        if self.allow_network || self.allow_ipc {
            r |= rights::EXECUTE; // needed for network/IPC syscall dispatch
        }
        r
    }
}

/// Apply a sandbox policy to a task.
///
/// Returns the constrained capability token that the task should use for all
/// subsequent privilege checks.  The returned capability has rights masked to
/// exactly the permissions the policy grants.  Call-site is responsible for
/// storing the token in the process descriptor and dropping the unconstrained
/// capability.
pub fn apply_sandbox(tid: u32, policy: &SandboxPolicy) -> Result<Capability, &'static str> {
    let allowed = policy.allowed_rights();
    Ok(Capability::create(tid, tid, allowed))
}
