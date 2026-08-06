//! AegisOS Security Subsystem
//!
//! Provides capability-based access control, sandboxing, audit logging,
//! keyring management, integrity checking, secure boot, and policy enforcement.

#![no_std]
#![deny(unsafe_op_in_unsafe_fn)]

pub mod audit;
pub mod capabilities;
pub mod hardening;
pub mod integrity;
pub mod keyring;
pub mod policy_engine;
pub mod sandbox;
pub mod secure_boot;

pub use capabilities::Capability;
pub use hardening::{HardeningControl, HardeningState};
pub use policy_engine::PolicyEngine;
