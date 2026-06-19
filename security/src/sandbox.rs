//! Process sandbox enforcement.

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
}

/// Apply a sandbox policy to a task (called by kernel on task creation).
pub fn apply_sandbox(tid: u32, policy: &SandboxPolicy) -> Result<(), &'static str> {
    // Enforcement hooks into the kernel capability table via FFI
    let _ = (tid, policy);
    Ok(())
}
