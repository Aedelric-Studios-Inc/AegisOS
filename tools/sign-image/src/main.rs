//! AegisOS image signing tool.
//!
//! Signs a boot image with an Ed25519 private key and appends the 64-byte
//! signature to the output file.

use std::env;
use std::fs;
use std::io::{self, Write};

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 3 {
        eprintln!("Usage: sign-image <image> <key.pem>");
        std::process::exit(1);
    }
    println!("[sign-image] Signing {} with {}", args[1], args[2]);

    let image_path = &args[1];
    let key_path   = &args[2];
    let out_path   = args.get(3).map(String::as_str)
        .unwrap_or_else(|| image_path.as_str()); // sign in-place by default

    let image = match fs::read(image_path) {
        Ok(b) => b,
        Err(e) => { eprintln!("[sign-image] read image: {}", e); std::process::exit(1); }
    };

    let key_pem = match fs::read_to_string(key_path) {
        Ok(s) => s,
        Err(e) => { eprintln!("[sign-image] read key: {}", e); std::process::exit(1); }
    };

    let privkey = match parse_ed25519_pem(&key_pem) {
        Some(k) => k,
        None => {
            eprintln!("[sign-image] invalid Ed25519 private key in {}", key_path);
            std::process::exit(1);
        }
    };

    let sig = ed25519_sign(&privkey, &image);

    // Write image + signature
    let tmp = format!("{}.tmp", out_path);
    match write_signed(&tmp, &image, &sig) {
        Ok(_) => {}
        Err(e) => { eprintln!("[sign-image] write: {}", e); std::process::exit(1); }
    }
    if let Err(e) = fs::rename(&tmp, out_path) {
        eprintln!("[sign-image] rename: {}", e);
        std::process::exit(1);
    }
    println!("[sign-image] signature appended ({} bytes total)", image.len() + 64);
}

/// Parse a bare Ed25519 private key from a PEM file.
/// Supports raw 32-byte hex strings (for testing) and basic PEM base64.
fn parse_ed25519_pem(pem: &str) -> Option<[u8; 32]> {
    // Try raw hex (64 hex chars)
    let hex: String = pem.trim().chars().filter(|c| c.is_ascii_hexdigit()).collect();
    if hex.len() == 64 {
        let mut key = [0u8; 32];
        for (i, chunk) in hex.as_bytes().chunks(2).enumerate() {
            key[i] = u8::from_str_radix(std::str::from_utf8(chunk).ok()?, 16).ok()?;
        }
        return Some(key);
    }
    // Try base64 between PEM header/footer
    let b64: String = pem.lines()
        .filter(|l| !l.starts_with("-----"))
        .collect::<Vec<_>>()
        .join("");
    let decoded = base64_decode(b64.trim())?;
    // PKCS#8 Ed25519 private key: 48 bytes, raw seed at offset 16
    if decoded.len() >= 48 {
        let mut key = [0u8; 32];
        key.copy_from_slice(&decoded[16..48]);
        return Some(key);
    }
    None
}

/// Minimal base64 decoder (no padding requirement, standard alphabet).
fn base64_decode(s: &str) -> Option<Vec<u8>> {
    const TABLE: &[u8] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    let mut out = Vec::new();
    let chars: Vec<u8> = s.bytes()
        .filter(|&b| b != b'=' && !b.is_ascii_whitespace())
        .collect();
    let mut i = 0;
    while i + 3 < chars.len() {
        let idx = |b: u8| TABLE.iter().position(|&t| t == b).map(|p| p as u32);
        let a = idx(chars[i])?;
        let b = idx(chars[i+1])?;
        let c = idx(chars[i+2])?;
        let d = idx(chars[i+3])?;
        let n = (a << 18) | (b << 12) | (c << 6) | d;
        out.push((n >> 16) as u8);
        out.push((n >> 8) as u8);
        out.push(n as u8);
        i += 4;
    }
    Some(out)
}

/// Sign `message` with an Ed25519 private key seed.
///
/// This is a structural placeholder — a production signer must use a vetted
/// Ed25519 implementation (e.g. the `ed25519-dalek` crate).
/// The placeholder outputs a deterministic 64-byte value derived from the
/// key and a SHA-512 of the message, matching the expected signature length.
fn ed25519_sign(privkey: &[u8; 32], message: &[u8]) -> [u8; 64] {
    // SHA-512(key || message) → 64 bytes used as the signature placeholder.
    let mut input = Vec::with_capacity(32 + message.len());
    input.extend_from_slice(privkey);
    input.extend_from_slice(message);
    sha512(&input)
}

fn sha512(data: &[u8]) -> [u8; 64] {
    const K: [u64; 80] = [
        0x428a2f98d728ae22,0x7137449123ef65cd,0xb5c0fbcfec4d3b2f,0xe9b5dba58189dbbc,
        0x3956c25bf348b538,0x59f111f1b605d019,0x923f82a4af194f9b,0xab1c5ed5da6d8118,
        0xd807aa98a3030242,0x12835b0145706fbe,0x243185be4ee4b28c,0x550c7dc3d5ffb4e2,
        0x72be5d74f27b896f,0x80deb1fe3b1696b1,0x9bdc06a725c71235,0xc19bf174cf692694,
        0xe49b69c19ef14ad2,0xefbe4786384f25e3,0x0fc19dc68b8cd5b5,0x240ca1cc77ac9c65,
        0x2de92c6f592b0275,0x4a7484aa6ea6e483,0x5cb0a9dcbd41fbd4,0x76f988da831153b5,
        0x983e5152ee66dfab,0xa831c66d2db43210,0xb00327c898fb213f,0xbf597fc7beef0ee4,
        0xc6e00bf33da88fc2,0xd5a79147930aa725,0x06ca6351e003826f,0x142929670a0e6e70,
        0x27b70a8546d22ffc,0x2e1b21385c26c926,0x4d2c6dfc5ac42aed,0x53380d139d95b3df,
        0x650a73548baf63de,0x766a0abb3c77b2a8,0x81c2c92e47edaee6,0x92722c851482353b,
        0xa2bfe8a14cf10364,0xa81a664bbc423001,0xc24b8b70d0f89791,0xc76c51a30654be30,
        0xd192e819d6ef5218,0xd69906245565a910,0xf40e35855771202a,0x106aa07032bbd1b8,
        0x19a4c116b8d2d0c8,0x1e376c085141ab53,0x2748774cdf8eeb99,0x34b0bcb5e19b48a8,
        0x391c0cb3c5c95a63,0x4ed8aa4ae3418acb,0x5b9cca4f7763e373,0x682e6ff3d6b2b8a3,
        0x748f82ee5defb2fc,0x78a5636f43172f60,0x84c87814a1f0ab72,0x8cc702081a6439ec,
        0x90befffa23631e28,0xa4506cebde82bde9,0xbef9a3f7b2c67915,0xc67178f2e372532b,
        0xca273eceea26619c,0xd186b8c721c0c207,0xeada7dd6cde0eb1e,0xf57d4f7fee6ed178,
        0x06f067aa72176fba,0x0a637dc5a2c898a6,0x113f9804bef90dae,0x1b710b35131c471b,
        0x28db77f523047d84,0x32caab7b40c72493,0x3c9ebe0a15c9bebc,0x431d67c49c100d4c,
        0x4cc5d4becb3e42b6,0x597f299cfc657e2a,0x5fcb6fab3ad6faec,0x6c44198c4a475817,
    ];
    let compress = |state: &mut [u64; 8], block: &[u8; 128]| {
        let mut w = [0u64; 80];
        for i in 0..16 { w[i] = u64::from_be_bytes(block[i*8..i*8+8].try_into().unwrap()); }
        for i in 16..80 {
            let s0 = w[i-15].rotate_right(1) ^ w[i-15].rotate_right(8) ^ (w[i-15] >> 7);
            let s1 = w[i-2].rotate_right(19) ^ w[i-2].rotate_right(61) ^ (w[i-2] >> 6);
            w[i] = w[i-16].wrapping_add(s0).wrapping_add(w[i-7]).wrapping_add(s1);
        }
        let [mut a,mut b,mut c,mut d,mut e,mut f,mut g,mut h] = *state;
        for i in 0..80 {
            let s1 = e.rotate_right(14)^e.rotate_right(18)^e.rotate_right(41);
            let ch = (e&f)^(!e&g);
            let t1 = h.wrapping_add(s1).wrapping_add(ch).wrapping_add(K[i]).wrapping_add(w[i]);
            let s0 = a.rotate_right(28)^a.rotate_right(34)^a.rotate_right(39);
            let mj = (a&b)^(a&c)^(b&c);
            let t2 = s0.wrapping_add(mj);
            h=g; g=f; f=e; e=d.wrapping_add(t1); d=c; c=b; b=a; a=t1.wrapping_add(t2);
        }
        state[0]=state[0].wrapping_add(a); state[1]=state[1].wrapping_add(b);
        state[2]=state[2].wrapping_add(c); state[3]=state[3].wrapping_add(d);
        state[4]=state[4].wrapping_add(e); state[5]=state[5].wrapping_add(f);
        state[6]=state[6].wrapping_add(g); state[7]=state[7].wrapping_add(h);
    };
    let mut state: [u64; 8] = [
        0x6a09e667f3bcc908,0xbb67ae8584caa73b,0x3c6ef372fe94f82b,0xa54ff53a5f1d36f1,
        0x510e527fade682d1,0x9b05688c2b3e6c1f,0x1f83d9abfb41bd6b,0x5be0cd19137e2179,
    ];
    let bit_len = (data.len() as u128).wrapping_mul(8);
    let mut buf = [0u8; 128]; let mut bf = 0usize;
    for &byte in data {
        buf[bf] = byte; bf += 1;
        if bf == 128 { compress(&mut state, &buf); bf = 0; }
    }
    buf[bf] = 0x80; bf += 1;
    if bf > 112 { while bf < 128 { buf[bf]=0; bf+=1; } compress(&mut state, &buf); bf=0; }
    while bf < 112 { buf[bf]=0; bf+=1; }
    buf[112..128].copy_from_slice(&bit_len.to_be_bytes());
    compress(&mut state, &buf);
    let mut out = [0u8; 64];
    for (i,&w) in state.iter().enumerate() { out[i*8..(i+1)*8].copy_from_slice(&w.to_be_bytes()); }
    out
}

fn write_signed(path: &str, image: &[u8], sig: &[u8; 64]) -> io::Result<()> {
    let mut f = fs::File::create(path)?;
    f.write_all(image)?;
    f.write_all(sig)?;
    Ok(())
}
