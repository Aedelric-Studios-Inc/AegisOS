//! Cryptographic keyring — key storage and management.

/// Key type discriminant.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum KeyType {
    Aes256,
    Ed25519Public,
    Ed25519Private,
    HmacSha256,
}

/// A stored key entry.
#[derive(Debug, Clone)]
pub struct KeyEntry {
    pub id: u32,
    pub key_type: KeyType,
    pub label: [u8; 32],
    pub data: [u8; 64],
    pub data_len: usize,
}

use core::cell::UnsafeCell;

struct KeyringState {
    keys: [Option<KeyEntry>; MAX_KEYS],
    next_key_id: u32,
}

struct KeyringCell(UnsafeCell<KeyringState>);

// SAFETY: this crate currently models the early boot security layer, where
// callers must serialise keyring access. A real kernel spinlock should replace
// this before SMP/pre-emptive access is enabled.
unsafe impl Sync for KeyringCell {}

const MAX_KEYS: usize = 64;

static KEYRING: KeyringCell = KeyringCell(UnsafeCell::new(KeyringState {
    keys: [const { None }; MAX_KEYS],
    next_key_id: 1,
}));

/// Store a key in the keyring.
pub fn keyring_store(key_type: KeyType, label: &[u8], data: &[u8]) -> Result<u32, &'static str> {
    if data.len() > 64 {
        return Err("key too large");
    }

    // SAFETY: early-boot single-writer access; replace with a kernel lock before SMP.
    let state = unsafe { &mut *KEYRING.0.get() };
    let slot_idx = state
        .keys
        .iter()
        .position(Option::is_none)
        .ok_or("keyring full")?;

    let id = state.next_key_id;
    state.next_key_id = state.next_key_id.checked_add(1).ok_or("key id overflow")?;

    let mut entry = KeyEntry {
        id,
        key_type,
        label: [0u8; 32],
        data: [0u8; 64],
        data_len: data.len(),
    };
    let label_len = label.len().min(32);
    entry.label[..label_len].copy_from_slice(&label[..label_len]);
    entry.data[..data.len()].copy_from_slice(data);
    state.keys[slot_idx] = Some(entry);

    Ok(id)
}

/// Look up a key by ID.
pub fn keyring_get(id: u32) -> Option<KeyEntry> {
    // SAFETY: early-boot read access; replace with a kernel lock before SMP.
    let state = unsafe { &*KEYRING.0.get() };
    state
        .keys
        .iter()
        .find_map(|slot| slot.as_ref().filter(|e| e.id == id).cloned())
}
