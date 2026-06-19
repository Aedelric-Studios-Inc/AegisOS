//! Password hashing and verification.
//!
//! Uses PBKDF2-SHA256 with 100_000 iterations.
//! A production deployment should prefer Argon2id; this implementation
//! avoids external crate dependencies while still being considerably
//! stronger than plain SHA-256.

const ITERATIONS: u32 = 100_000;
const SALT_LEN: usize = 16;
const KEY_LEN:  usize = 32;

/// Hash a password, returning "{iterations}${hex_salt}${hex_hash}".
pub fn hash(password: &str) -> String {
    let salt = generate_salt();
    let dk   = pbkdf2_sha256(password.as_bytes(), &salt, ITERATIONS, KEY_LEN);
    format!("{}${}${}", ITERATIONS, hex(&salt), hex(&dk))
}

/// Verify a password against a stored hash string.
pub fn verify(password: &str, stored: &str) -> bool {
    let parts: Vec<&str> = stored.splitn(3, '$').collect();
    if parts.len() != 3 { return false; }
    let iters: u32 = match parts[0].parse() { Ok(n) => n, Err(_) => return false };
    let salt  = match unhex(parts[1]) { Some(s) => s, None => return false };
    let want  = match unhex(parts[2]) { Some(h) => h, None => return false };
    let got   = pbkdf2_sha256(password.as_bytes(), &salt, iters, want.len());
    // Constant-time comparison.
    ct_eq(&got, &want)
}

fn ct_eq(a: &[u8], b: &[u8]) -> bool {
    if a.len() != b.len() { return false; }
    let mut diff = 0u8;
    for (&x, &y) in a.iter().zip(b.iter()) { diff |= x ^ y; }
    diff == 0
}

fn generate_salt() -> [u8; SALT_LEN] {
    let t = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default()
        .as_nanos() as u64;
    let mut s = [0u8; SALT_LEN];
    for (i, b) in s.iter_mut().enumerate() {
        *b = ((t >> (i % 8)) ^ (i as u64 * 0x9e37)) as u8;
    }
    s
}

fn hex(b: &[u8]) -> String { b.iter().map(|x| format!("{:02x}", x)).collect() }

fn unhex(s: &str) -> Option<Vec<u8>> {
    if s.len() % 2 != 0 { return None; }
    s.as_bytes().chunks(2).map(|c| {
        let a = c[0] as char;
        let b = c[1] as char;
        let hi = a.to_digit(16)? as u8;
        let lo = b.to_digit(16)? as u8;
        Some((hi << 4) | lo)
    }).collect()
}

// ---- Minimal PBKDF2-HMAC-SHA256 ----

fn pbkdf2_sha256(password: &[u8], salt: &[u8], iters: u32, dklen: usize) -> Vec<u8> {
    let mut dk = Vec::new();
    let mut block = 1u32;
    while dk.len() < dklen {
        let mut u = hmac_sha256(password, &prf_input(salt, block));
        let mut t = u;
        for _ in 1..iters {
            u = hmac_sha256(password, &u);
            for (ti, ui) in t.iter_mut().zip(u.iter()) { *ti ^= ui; }
        }
        dk.extend_from_slice(&t);
        block += 1;
    }
    dk.truncate(dklen);
    dk
}

fn prf_input(salt: &[u8], block: u32) -> Vec<u8> {
    let mut v = salt.to_vec();
    v.extend_from_slice(&block.to_be_bytes());
    v
}

fn hmac_sha256(key: &[u8], msg: &[u8]) -> [u8; 32] {
    let k = if key.len() > 64 { sha256(key).to_vec() } else { key.to_vec() };
    let mut k_padded = [0u8; 64];
    k_padded[..k.len()].copy_from_slice(&k);
    let ipad: Vec<u8> = k_padded.iter().map(|b| b ^ 0x36).collect();
    let opad: Vec<u8> = k_padded.iter().map(|b| b ^ 0x5c).collect();
    let inner: Vec<u8> = ipad.iter().chain(msg.iter()).copied().collect();
    let inner_hash = sha256(&inner);
    let outer: Vec<u8> = opad.iter().chain(inner_hash.iter()).copied().collect();
    sha256(&outer)
}

fn sha256(data: &[u8]) -> [u8; 32] {
    const K: [u32; 64] = [
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2,
    ];
    let mut state = [
        0x6a09e667u32, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f,    0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
    ];
    let bit_len  = (data.len() as u64).wrapping_mul(8);
    let mut padded = data.to_vec();
    padded.push(0x80);
    while padded.len() % 64 != 56 { padded.push(0); }
    padded.extend_from_slice(&bit_len.to_be_bytes());
    for chunk in padded.chunks(64) {
        let mut w = [0u32; 64];
        for i in 0..16 {
            w[i] = u32::from_be_bytes(chunk[i*4..i*4+4].try_into().unwrap());
        }
        for i in 16..64 {
            let s0 = w[i-15].rotate_right(7)^w[i-15].rotate_right(18)^(w[i-15]>>3);
            let s1 = w[i-2].rotate_right(17)^w[i-2].rotate_right(19)^(w[i-2]>>10);
            w[i] = w[i-16].wrapping_add(s0).wrapping_add(w[i-7]).wrapping_add(s1);
        }
        let [mut a,mut b,mut c,mut d,mut e,mut f,mut g,mut h] = state;
        for i in 0..64 {
            let s1 = e.rotate_right(6)^e.rotate_right(11)^e.rotate_right(25);
            let ch = (e&f)^(!e&g);
            let t1 = h.wrapping_add(s1).wrapping_add(ch).wrapping_add(K[i]).wrapping_add(w[i]);
            let s0 = a.rotate_right(2)^a.rotate_right(13)^a.rotate_right(22);
            let mj = (a&b)^(a&c)^(b&c);
            let t2 = s0.wrapping_add(mj);
            h=g;g=f;f=e;e=d.wrapping_add(t1);d=c;c=b;b=a;a=t1.wrapping_add(t2);
        }
        state[0]=state[0].wrapping_add(a); state[1]=state[1].wrapping_add(b);
        state[2]=state[2].wrapping_add(c); state[3]=state[3].wrapping_add(d);
        state[4]=state[4].wrapping_add(e); state[5]=state[5].wrapping_add(f);
        state[6]=state[6].wrapping_add(g); state[7]=state[7].wrapping_add(h);
    }
    let mut out = [0u8; 32];
    for (i, &s) in state.iter().enumerate() { out[i*4..i*4+4].copy_from_slice(&s.to_be_bytes()); }
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn round_trip() {
        let h = hash("correct-horse-battery-staple");
        assert!(verify("correct-horse-battery-staple", &h));
        assert!(!verify("wrong-password", &h));
    }
}
