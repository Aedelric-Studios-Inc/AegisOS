//! Runtime policy engine.
//!
//! Evaluates fine-grained rules that control what services and processes may do.

use crate::audit::{audit_log, AuditEvent};

/// A single policy rule.
#[derive(Debug, Clone)]
pub struct PolicyRule {
    pub subject_tid: u32,
    pub action:      PolicyAction,
    pub resource:    PolicyResource,
    pub decision:    PolicyDecision,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PolicyAction {
    Read,
    Write,
    Execute,
    Connect,
    Bind,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PolicyResource {
    File,
    Network,
    Ipc,
    Capability,
    Any,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PolicyDecision {
    Allow,
    Deny,
}

/// Policy engine holding a set of rules.
pub struct PolicyEngine {
    rules: [Option<PolicyRule>; 256],
    count: usize,
}

impl PolicyEngine {
    pub const fn new() -> Self {
        Self {
            rules: [const { None }; 256],
            count: 0,
        }
    }

    pub fn add_rule(&mut self, rule: PolicyRule) -> Result<(), &'static str> {
        if self.count >= 256 { return Err("policy table full"); }
        self.rules[self.count] = Some(rule);
        self.count += 1;
        Ok(())
    }

    /// Evaluate a request. Returns `Deny` if no matching rule is found (default deny).
    pub fn evaluate(
        &self,
        tid: u32,
        action: PolicyAction,
        resource: PolicyResource,
    ) -> PolicyDecision {
        for slot in &self.rules[..self.count] {
            if let Some(rule) = slot {
                let matches_subject  = rule.subject_tid == tid || rule.subject_tid == 0;
                let matches_action   = rule.action == action;
                let matches_resource = rule.resource == resource || rule.resource == PolicyResource::Any;
                if matches_subject && matches_action && matches_resource {
                    if rule.decision == PolicyDecision::Deny {
                        audit_log(AuditEvent::PolicyDenied, tid, 0);
                    }
                    return rule.decision;
                }
            }
        }
        // Default-deny: no matching rule means the action is not explicitly permitted.
        audit_log(AuditEvent::PolicyDenied, tid, 0);
        PolicyDecision::Deny
    }
}
