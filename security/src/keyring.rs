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
#[derive(Debug)]
pub struct KeyEntry {
    pub id:       u32,
    pub key_type: KeyType,
    pub label:    [u8; 32],
    pub data:     [u8; 64],
    pub data_len: usize,
}

const MAX_KEYS: usize = 64;
static mut KEYRING: [Option<KeyEntry>; MAX_KEYS] = [const { None }; MAX_KEYS];
static mut NEXT_KEY_ID: u32 = 1;

/// Store a key in the keyring.
pub fn keyring_store(key_type: KeyType, label: &[u8], data: &[u8]) -> Result<u32, &'static str> {
    if data.len() > 64 { return Err("key too large"); }
    // SAFETY: single-threaded early init context
    unsafe {
        for slot in KEYRING.iter_mut() {
            if slot.is_none() {
                let mut entry = KeyEntry {
                    id: NEXT_KEY_ID,
                    key_type,
                    label: [0u8; 32],
                    data: [0u8; 64],
                    data_len: data.len(),
                };
                let label_len = label.len().min(32);
                entry.label[..label_len].copy_from_slice(&label[..label_len]);
                entry.data[..data.len()].copy_from_slice(data);
                let id = NEXT_KEY_ID;
                NEXT_KEY_ID += 1;
                *slot = Some(entry);
                return Ok(id);
            }
        }
    }
    Err("keyring full")
}

/// Look up a key by ID.
pub fn keyring_get(id: u32) -> Option<&'static KeyEntry> {
    // SAFETY: read-only access
    unsafe {
        KEYRING.iter().find_map(|slot| {
            slot.as_ref().filter(|e| e.id == id)
        })
    }
}
