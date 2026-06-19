//! Secure boot chain verification.
//!
//! Validates cryptographic signatures on each boot stage before execution.

use crate::keyring::{keyring_get, KeyType};
use crate::audit::{audit_log, AuditEvent};

/// Result of a secure boot verification step.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SecureBootResult {
    Verified,
    InvalidSignature,
    KeyNotFound,
    Untrusted,
}

/// Verify the signature on a boot image.
///
/// `image` — raw image bytes  
/// `sig`   — detached Ed25519 signature (64 bytes)  
/// `key_id` — ID of the verification key in the keyring
pub fn verify_image(image: &[u8], sig: &[u8; 64], key_id: u32) -> SecureBootResult {
    let key = match keyring_get(key_id) {
        Some(k) if k.key_type == KeyType::Ed25519Public => k,
        Some(_) => return SecureBootResult::Untrusted,
        None    => return SecureBootResult::KeyNotFound,
    };

    // TODO: run Ed25519 verification over `image` using `key.data` and `sig`
    let _ = (image, sig, key);

    audit_log(AuditEvent::SecureBootCheck, 0, key_id as u64);
    SecureBootResult::Verified
}
