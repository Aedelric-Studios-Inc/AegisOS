//! AegisOS Security Subsystem
//!
//! Provides capability-based access control, sandboxing, audit logging,
//! keyring management, integrity checking, secure boot, and policy enforcement.

#![no_std]
#![deny(unsafe_op_in_unsafe_fn)]

pub mod capabilities;
pub mod sandbox;
pub mod audit;
pub mod keyring;
pub mod integrity;
pub mod secure_boot;
pub mod policy_engine;

pub use capabilities::Capability;
pub use policy_engine::PolicyEngine;
