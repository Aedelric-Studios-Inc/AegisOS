//! Binary and filesystem integrity verification.
//!
//! Uses SHA-256 for hash verification.

/// Result of an integrity check.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum IntegrityResult {
    Ok,
    Tampered,
    Missing,
}

// ---- Minimal SHA-256 implementation (no_std) ----

const K: [u32; 64] = [
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
];

fn sha256_compress(state: &mut [u32; 8], block: &[u8; 64]) {
    let mut w = [0u32; 64];
    for i in 0..16 {
        w[i] = u32::from_be_bytes([
            block[i * 4],
            block[i * 4 + 1],
            block[i * 4 + 2],
            block[i * 4 + 3],
        ]);
    }
    for i in 16..64 {
        let s0 = w[i - 15].rotate_right(7) ^ w[i - 15].rotate_right(18) ^ (w[i - 15] >> 3);
        let s1 = w[i - 2].rotate_right(17) ^ w[i - 2].rotate_right(19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16]
            .wrapping_add(s0)
            .wrapping_add(w[i - 7])
            .wrapping_add(s1);
    }
    let [mut a, mut b, mut c, mut d, mut e, mut f, mut g, mut h] = *state;
    for i in 0..64 {
        let s1 = e.rotate_right(6) ^ e.rotate_right(11) ^ e.rotate_right(25);
        let ch = (e & f) ^ (!e & g);
        let temp1 = h
            .wrapping_add(s1)
            .wrapping_add(ch)
            .wrapping_add(K[i])
            .wrapping_add(w[i]);
        let s0 = a.rotate_right(2) ^ a.rotate_right(13) ^ a.rotate_right(22);
        let maj = (a & b) ^ (a & c) ^ (b & c);
        let temp2 = s0.wrapping_add(maj);
        h = g;
        g = f;
        f = e;
        e = d.wrapping_add(temp1);
        d = c;
        c = b;
        b = a;
        a = temp1.wrapping_add(temp2);
    }
    state[0] = state[0].wrapping_add(a);
    state[1] = state[1].wrapping_add(b);
    state[2] = state[2].wrapping_add(c);
    state[3] = state[3].wrapping_add(d);
    state[4] = state[4].wrapping_add(e);
    state[5] = state[5].wrapping_add(f);
    state[6] = state[6].wrapping_add(g);
    state[7] = state[7].wrapping_add(h);
}

/// Compute the SHA-256 digest of `data`.
pub fn sha256(data: &[u8]) -> [u8; 32] {
    let mut state: [u32; 8] = [
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab,
        0x5be0cd19,
    ];

    let bit_len = (data.len() as u64).wrapping_mul(8);
    let mut buf = [0u8; 64];
    let mut offset = 0usize;
    let mut buf_fill = 0usize;

    // Process full blocks
    for &byte in data {
        buf[buf_fill] = byte;
        buf_fill += 1;
        offset += 1;
        if buf_fill == 64 {
            sha256_compress(&mut state, &buf);
            buf_fill = 0;
        }
    }

    // Padding
    buf[buf_fill] = 0x80;
    buf_fill += 1;
    if buf_fill > 56 {
        while buf_fill < 64 {
            buf[buf_fill] = 0;
            buf_fill += 1;
        }
        sha256_compress(&mut state, &buf);
        buf_fill = 0;
    }
    while buf_fill < 56 {
        buf[buf_fill] = 0;
        buf_fill += 1;
    }
    let _ = offset;
    let len_bytes = bit_len.to_be_bytes();
    buf[56..64].copy_from_slice(&len_bytes);
    sha256_compress(&mut state, &buf);

    let mut out = [0u8; 32];
    for (i, &word) in state.iter().enumerate() {
        out[i * 4..(i + 1) * 4].copy_from_slice(&word.to_be_bytes());
    }
    out
}

/// Verify that `data` matches the expected SHA-256 `expected_hash`.
pub fn verify_hash(data: &[u8], expected_hash: &[u8; 32]) -> IntegrityResult {
    let actual = sha256(data);
    if &actual == expected_hash {
        IntegrityResult::Ok
    } else {
        IntegrityResult::Tampered
    }
}

/// Verify the integrity of a file at the given path using a stored hash.
/// On AegisOS the kernel VFS is not directly accessible from Rust userspace
/// at this layer, so this function is provided as a hook for future integration.
pub fn verify_file(_path: &str, _expected_hash: &[u8; 32]) -> IntegrityResult {
    // VFS integration is wired through the syscall layer; this crate operates
    // at a level where the file contents must be supplied by the caller.
    IntegrityResult::Missing
}
