//! Security audit logging.

/// Categories of auditable events.
#[repr(u8)]
#[derive(Debug, Clone, Copy)]
pub enum AuditEvent {
    TaskCreated      = 0,
    TaskDestroyed    = 1,
    CapabilityGranted = 2,
    CapabilityRevoked = 3,
    SandboxViolation = 4,
    SecureBootCheck  = 5,
    IntegrityFailure = 6,
    PolicyDenied     = 7,
}

/// Audit log entry.
#[derive(Debug)]
pub struct AuditEntry {
    pub timestamp_ticks: u64,
    pub event:           AuditEvent,
    pub tid:             u32,
    pub detail:          u64,
}

/// Ring buffer for audit entries (no_std-compatible).
const AUDIT_BUF_SIZE: usize = 512;

static mut AUDIT_BUF: [Option<AuditEntry>; AUDIT_BUF_SIZE] = [const { None }; AUDIT_BUF_SIZE];
static mut AUDIT_HEAD: usize = 0;

/// Record a new audit event.
pub fn audit_log(event: AuditEvent, tid: u32, detail: u64) {
    // SAFETY: single-core kernel, no concurrent access during early boot
    unsafe {
        AUDIT_BUF[AUDIT_HEAD % AUDIT_BUF_SIZE] = Some(AuditEntry {
            timestamp_ticks: 0, // TODO: read ARM generic timer
            event,
            tid,
            detail,
        });
        AUDIT_HEAD = AUDIT_HEAD.wrapping_add(1);
    }
}
