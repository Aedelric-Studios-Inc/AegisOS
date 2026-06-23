//! Hardened baseline controls for AegisBox-class devices.
//!
//! This module intentionally models Security Operations controls rather than
//! product UI.  It gives the kernel/core a compact, no_std-friendly vocabulary
//! for baseline enforcement, audit triage, and secure configuration drift checks.

use crate::audit::{audit_log, AuditEvent};

/// Individual hardening controls that must remain enabled on production builds.
#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum HardeningControl {
    SecureBoot = 0,
    SignedUpdates = 1,
    AuditLogging = 2,
    CapabilityMode = 3,
    DefaultDenyFirewall = 4,
    ServiceAllowList = 5,
    LeastPrivilegeServices = 6,
    AdminTokenRequired = 7,
    SecretsOutsideWebRoot = 8,
    IntegrityVerification = 9,
    IntrusionWatch = 10,
    RecoveryPartition = 11,
}

impl HardeningControl {
    pub const fn as_str(self) -> &'static str {
        match self {
            HardeningControl::SecureBoot => "secure_boot",
            HardeningControl::SignedUpdates => "signed_updates",
            HardeningControl::AuditLogging => "audit_logging",
            HardeningControl::CapabilityMode => "capability_mode",
            HardeningControl::DefaultDenyFirewall => "default_deny_firewall",
            HardeningControl::ServiceAllowList => "service_allow_list",
            HardeningControl::LeastPrivilegeServices => "least_privilege_services",
            HardeningControl::AdminTokenRequired => "admin_token_required",
            HardeningControl::SecretsOutsideWebRoot => "secrets_outside_web_root",
            HardeningControl::IntegrityVerification => "integrity_verification",
            HardeningControl::IntrusionWatch => "intrusion_watch",
            HardeningControl::RecoveryPartition => "recovery_partition",
        }
    }
}

/// Baseline state as evaluated by the boot path, service manager, or daemon.
#[derive(Debug, Clone, Copy)]
pub struct HardeningState {
    pub secure_boot: bool,
    pub signed_updates: bool,
    pub audit_logging: bool,
    pub strict_capabilities: bool,
    pub default_deny_firewall: bool,
    pub service_allow_list: bool,
    pub least_privilege_services: bool,
    pub admin_token_required: bool,
    pub secrets_outside_web_root: bool,
    pub integrity_verification: bool,
    pub intrusion_watch: bool,
    pub recovery_partition: bool,
}

impl HardeningState {
    /// Production default: every baseline control must be enabled.
    pub const fn production() -> Self {
        Self {
            secure_boot: true,
            signed_updates: true,
            audit_logging: true,
            strict_capabilities: true,
            default_deny_firewall: true,
            service_allow_list: true,
            least_privilege_services: true,
            admin_token_required: true,
            secrets_outside_web_root: true,
            integrity_verification: true,
            intrusion_watch: true,
            recovery_partition: true,
        }
    }

    /// Bring-up default for lab boards.  This is deliberately weaker and must
    /// never be used for retail/customer AegisBox images.
    pub const fn development() -> Self {
        Self {
            secure_boot: false,
            signed_updates: false,
            audit_logging: true,
            strict_capabilities: true,
            default_deny_firewall: true,
            service_allow_list: true,
            least_privilege_services: false,
            admin_token_required: false,
            secrets_outside_web_root: true,
            integrity_verification: false,
            intrusion_watch: false,
            recovery_partition: false,
        }
    }

    pub fn enabled(self, control: HardeningControl) -> bool {
        match control {
            HardeningControl::SecureBoot => self.secure_boot,
            HardeningControl::SignedUpdates => self.signed_updates,
            HardeningControl::AuditLogging => self.audit_logging,
            HardeningControl::CapabilityMode => self.strict_capabilities,
            HardeningControl::DefaultDenyFirewall => self.default_deny_firewall,
            HardeningControl::ServiceAllowList => self.service_allow_list,
            HardeningControl::LeastPrivilegeServices => self.least_privilege_services,
            HardeningControl::AdminTokenRequired => self.admin_token_required,
            HardeningControl::SecretsOutsideWebRoot => self.secrets_outside_web_root,
            HardeningControl::IntegrityVerification => self.integrity_verification,
            HardeningControl::IntrusionWatch => self.intrusion_watch,
            HardeningControl::RecoveryPartition => self.recovery_partition,
        }
    }

    pub fn missing_count(self) -> u8 {
        let controls = HARDENING_CONTROLS;
        let mut missing = 0u8;
        let mut i = 0;
        while i < controls.len() {
            if !self.enabled(controls[i]) {
                missing = missing.saturating_add(1);
            }
            i += 1;
        }
        missing
    }

    pub fn is_production_ready(self) -> bool {
        self.missing_count() == 0
    }
}

pub const HARDENING_CONTROLS: [HardeningControl; 12] = [
    HardeningControl::SecureBoot,
    HardeningControl::SignedUpdates,
    HardeningControl::AuditLogging,
    HardeningControl::CapabilityMode,
    HardeningControl::DefaultDenyFirewall,
    HardeningControl::ServiceAllowList,
    HardeningControl::LeastPrivilegeServices,
    HardeningControl::AdminTokenRequired,
    HardeningControl::SecretsOutsideWebRoot,
    HardeningControl::IntegrityVerification,
    HardeningControl::IntrusionWatch,
    HardeningControl::RecoveryPartition,
];

/// Evaluate a baseline and emit an audit event if it drifts from production.
pub fn evaluate_baseline(tid: u32, state: HardeningState) -> bool {
    let ready = state.is_production_ready();
    if !ready {
        audit_log(AuditEvent::PolicyDenied, tid, state.missing_count() as u64);
    }
    ready
}

/// Security Operations triage severity.
#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum Severity {
    Info = 0,
    Low = 1,
    Medium = 2,
    High = 3,
    Critical = 4,
}

/// Classify an audit event for dashboards/SIEM-style routing.
pub fn classify_event(event: AuditEvent) -> Severity {
    match event {
        AuditEvent::IntegrityFailure => Severity::Critical,
        AuditEvent::SecureBootCheck => Severity::High,
        AuditEvent::SandboxViolation => Severity::High,
        AuditEvent::PolicyDenied => Severity::Medium,
        AuditEvent::CapabilityGranted => Severity::Low,
        AuditEvent::CapabilityRevoked => Severity::Low,
        AuditEvent::TaskCreated | AuditEvent::TaskDestroyed => Severity::Info,
    }
}
