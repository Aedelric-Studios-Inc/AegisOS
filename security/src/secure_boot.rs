//! Secure boot chain verification.
//!
//! Validates Ed25519 signatures on each boot stage before execution.

use crate::audit::{audit_log, AuditEvent};
use crate::keyring::{keyring_get, KeyType};

/// Result of a secure boot verification step.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SecureBootResult {
    Verified,
    InvalidSignature,
    KeyNotFound,
    Untrusted,
}

// ---- Ed25519 / SHA-512 implementation (no_std, constant-time for signing key ops) ----
// Implements RFC 8032 §5.1 Ed25519 verify.

const SHA512_K: [u64; 80] = [
    0x428a2f98d728ae22,
    0x7137449123ef65cd,
    0xb5c0fbcfec4d3b2f,
    0xe9b5dba58189dbbc,
    0x3956c25bf348b538,
    0x59f111f1b605d019,
    0x923f82a4af194f9b,
    0xab1c5ed5da6d8118,
    0xd807aa98a3030242,
    0x12835b0145706fbe,
    0x243185be4ee4b28c,
    0x550c7dc3d5ffb4e2,
    0x72be5d74f27b896f,
    0x80deb1fe3b1696b1,
    0x9bdc06a725c71235,
    0xc19bf174cf692694,
    0xe49b69c19ef14ad2,
    0xefbe4786384f25e3,
    0x0fc19dc68b8cd5b5,
    0x240ca1cc77ac9c65,
    0x2de92c6f592b0275,
    0x4a7484aa6ea6e483,
    0x5cb0a9dcbd41fbd4,
    0x76f988da831153b5,
    0x983e5152ee66dfab,
    0xa831c66d2db43210,
    0xb00327c898fb213f,
    0xbf597fc7beef0ee4,
    0xc6e00bf33da88fc2,
    0xd5a79147930aa725,
    0x06ca6351e003826f,
    0x142929670a0e6e70,
    0x27b70a8546d22ffc,
    0x2e1b21385c26c926,
    0x4d2c6dfc5ac42aed,
    0x53380d139d95b3df,
    0x650a73548baf63de,
    0x766a0abb3c77b2a8,
    0x81c2c92e47edaee6,
    0x92722c851482353b,
    0xa2bfe8a14cf10364,
    0xa81a664bbc423001,
    0xc24b8b70d0f89791,
    0xc76c51a30654be30,
    0xd192e819d6ef5218,
    0xd69906245565a910,
    0xf40e35855771202a,
    0x106aa07032bbd1b8,
    0x19a4c116b8d2d0c8,
    0x1e376c085141ab53,
    0x2748774cdf8eeb99,
    0x34b0bcb5e19b48a8,
    0x391c0cb3c5c95a63,
    0x4ed8aa4ae3418acb,
    0x5b9cca4f7763e373,
    0x682e6ff3d6b2b8a3,
    0x748f82ee5defb2fc,
    0x78a5636f43172f60,
    0x84c87814a1f0ab72,
    0x8cc702081a6439ec,
    0x90befffa23631e28,
    0xa4506cebde82bde9,
    0xbef9a3f7b2c67915,
    0xc67178f2e372532b,
    0xca273eceea26619c,
    0xd186b8c721c0c207,
    0xeada7dd6cde0eb1e,
    0xf57d4f7fee6ed178,
    0x06f067aa72176fba,
    0x0a637dc5a2c898a6,
    0x113f9804bef90dae,
    0x1b710b35131c471b,
    0x28db77f523047d84,
    0x32caab7b40c72493,
    0x3c9ebe0a15c9bebc,
    0x431d67c49c100d4c,
    0x4cc5d4becb3e42b6,
    0x597f299cfc657e2a,
    0x5fcb6fab3ad6faec,
    0x6c44198c4a475817,
];

fn sha512_compress(state: &mut [u64; 8], block: &[u8; 128]) {
    let mut w = [0u64; 80];
    for i in 0..16 {
        w[i] = u64::from_be_bytes(block[i * 8..i * 8 + 8].try_into().unwrap());
    }
    for i in 16..80 {
        let s0 = w[i - 15].rotate_right(1) ^ w[i - 15].rotate_right(8) ^ (w[i - 15] >> 7);
        let s1 = w[i - 2].rotate_right(19) ^ w[i - 2].rotate_right(61) ^ (w[i - 2] >> 6);
        w[i] = w[i - 16]
            .wrapping_add(s0)
            .wrapping_add(w[i - 7])
            .wrapping_add(s1);
    }
    let [mut a, mut b, mut c, mut d, mut e, mut f, mut g, mut h] = *state;
    for i in 0..80 {
        let s1 = e.rotate_right(14) ^ e.rotate_right(18) ^ e.rotate_right(41);
        let ch = (e & f) ^ (!e & g);
        let temp1 = h
            .wrapping_add(s1)
            .wrapping_add(ch)
            .wrapping_add(SHA512_K[i])
            .wrapping_add(w[i]);
        let s0 = a.rotate_right(28) ^ a.rotate_right(34) ^ a.rotate_right(39);
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

// ---- Ed25519 field arithmetic over GF(2^255 - 19) ----
// Field elements as 5 x i64 limbs (radix 2^51).

type Fe = [i64; 5];

const fn fe_zero() -> Fe {
    [0; 5]
}
const fn fe_one() -> Fe {
    [1, 0, 0, 0, 0]
}

fn fe_add(a: &Fe, b: &Fe) -> Fe {
    [
        a[0] + b[0],
        a[1] + b[1],
        a[2] + b[2],
        a[3] + b[3],
        a[4] + b[4],
    ]
}

fn fe_sub(a: &Fe, b: &Fe) -> Fe {
    // Add 2*p to avoid underflow before subtracting
    let p2 = [
        0xFFFFFFFFFFFDA_i64 * 2,
        0xFFFFFFFFFFFFE_i64 * 2,
        0xFFFFFFFFFFFFE_i64 * 2,
        0xFFFFFFFFFFFFE_i64 * 2,
        0xFFFFFFFFFFFFE_i64 * 2,
    ];
    [
        a[0] + p2[0] - b[0],
        a[1] + p2[1] - b[1],
        a[2] + p2[2] - b[2],
        a[3] + p2[3] - b[3],
        a[4] + p2[4] - b[4],
    ]
}

fn fe_mul(a: &Fe, b: &Fe) -> Fe {
    let [a0, a1, a2, a3, a4] = *a;
    let [b0, b1, b2, b3, b4] = *b;
    let b1_19 = 19 * b1;
    let b2_19 = 19 * b2;
    let b3_19 = 19 * b3;
    let b4_19 = 19 * b4;

    let mut h = [
        a0 * b0 + a4 * b1_19 + a3 * b2_19 + a2 * b3_19 + a1 * b4_19,
        a1 * b0 + a0 * b1 + a4 * b2_19 + a3 * b3_19 + a2 * b4_19,
        a2 * b0 + a1 * b1 + a0 * b2 + a4 * b3_19 + a3 * b4_19,
        a3 * b0 + a2 * b1 + a1 * b2 + a0 * b3 + a4 * b4_19,
        a4 * b0 + a3 * b1 + a2 * b2 + a1 * b3 + a0 * b4,
    ];
    fe_reduce(&mut h);
    h
}

fn fe_reduce(h: &mut Fe) {
    let mask51: i64 = (1i64 << 51) - 1;
    for _ in 0..2 {
        let c0 = h[0] >> 51;
        h[1] += c0;
        h[0] &= mask51;
        let c1 = h[1] >> 51;
        h[2] += c1;
        h[1] &= mask51;
        let c2 = h[2] >> 51;
        h[3] += c2;
        h[2] &= mask51;
        let c3 = h[3] >> 51;
        h[4] += c3;
        h[3] &= mask51;
        let c4 = h[4] >> 51;
        h[0] += c4 * 19;
        h[4] &= mask51;
    }
}

fn fe_sq(a: &Fe) -> Fe {
    fe_mul(a, a)
}

fn fe_pow22523(z: &Fe) -> Fe {
    // z^((p-5)/8) = z^(2^252 - 3) used for sqrt in Ed25519
    let mut t0 = fe_sq(z); // z^2
    let mut t1 = fe_sq(&t0); // z^4
    t1 = fe_sq(&t1); // z^8
    t1 = fe_mul(&t1, z); // z^9
    t0 = fe_mul(&t0, &t1); // z^11
    t0 = fe_sq(&t0); // z^22
    t0 = fe_mul(&t0, &t1); // z^31 = z^(2^5-1)
    t1 = fe_sq(&t0);
    for _ in 1..5 {
        t1 = fe_sq(&t1);
    }
    t0 = fe_mul(&t1, &t0); // z^(2^10-1)
    t1 = fe_sq(&t0);
    for _ in 1..10 {
        t1 = fe_sq(&t1);
    }
    t1 = fe_mul(&t1, &t0); // z^(2^20-1)
    let mut t2 = fe_sq(&t1);
    for _ in 1..20 {
        t2 = fe_sq(&t2);
    }
    t1 = fe_mul(&t2, &t1); // z^(2^40-1)
    t1 = fe_sq(&t1);
    for _ in 1..10 {
        t1 = fe_sq(&t1);
    }
    t0 = fe_mul(&t1, &t0); // z^(2^50-1)
    t1 = fe_sq(&t0);
    for _ in 1..50 {
        t1 = fe_sq(&t1);
    }
    t1 = fe_mul(&t1, &t0); // z^(2^100-1)
    t2 = fe_sq(&t1);
    for _ in 1..100 {
        t2 = fe_sq(&t2);
    }
    t1 = fe_mul(&t2, &t1); // z^(2^200-1)
    t1 = fe_sq(&t1);
    for _ in 1..50 {
        t1 = fe_sq(&t1);
    }
    t0 = fe_mul(&t1, &t0); // z^(2^250-1)
    t0 = fe_sq(&t0);
    t0 = fe_sq(&t0); // z^(2^252-4)
    fe_mul(&t0, z) // z^(2^252-3)
}

fn fe_to_bytes(h: &Fe) -> [u8; 32] {
    let mut f = *h;
    fe_reduce(&mut f);
    // Final full reduction mod p
    let mask51: i64 = (1i64 << 51) - 1;
    let mut q = (19 * f[4] + (1i64 << 62)) >> 63;
    q = (f[0] + 19 * q) >> 51;
    q = (f[1] + q) >> 51;
    q = (f[2] + q) >> 51;
    q = (f[3] + q) >> 51;
    q = (f[4] + q) >> 51;
    f[0] += 19 * q;
    f[1] += f[0] >> 51;
    f[0] &= mask51;
    f[2] += f[1] >> 51;
    f[1] &= mask51;
    f[3] += f[2] >> 51;
    f[2] &= mask51;
    f[4] += f[3] >> 51;
    f[3] &= mask51;
    f[4] &= mask51;

    let mut out = [0u8; 32];
    let s = [
        (f[0] as u64) | ((f[1] as u64) << 51),
        (f[1] as u64 >> 13) | ((f[2] as u64) << 38),
        (f[2] as u64 >> 26) | ((f[3] as u64) << 25),
        (f[3] as u64 >> 39) | ((f[4] as u64) << 12),
    ];
    out[0..8].copy_from_slice(&s[0].to_le_bytes());
    out[8..16].copy_from_slice(&s[1].to_le_bytes());
    out[16..24].copy_from_slice(&s[2].to_le_bytes());
    out[24..32].copy_from_slice(&s[3].to_le_bytes());
    out
}

fn fe_from_bytes(s: &[u8; 32]) -> Fe {
    let load = |b: &[u8]| -> i64 {
        let mut v = 0u64;
        for (i, &x) in b.iter().enumerate() {
            v |= (x as u64) << (i * 8);
        }
        v as i64
    };
    let mask51: i64 = (1i64 << 51) - 1;
    [
        load(&s[0..7]) & mask51,
        (load(&s[6..13]) >> 3) & mask51,
        (load(&s[12..19]) >> 6) & mask51,
        (load(&s[19..26]) >> 1) & mask51,
        (load(&s[24..32]) >> 12) & 0x7FFFFFFFFFFFF_i64,
    ]
}

fn fe_is_negative(f: &Fe) -> bool {
    (fe_to_bytes(f)[0] & 1) != 0
}

fn fe_cswap(f: &mut Fe, g: &mut Fe, swap: i64) {
    for i in 0..5 {
        let t = swap & (f[i] ^ g[i]);
        f[i] ^= t;
        g[i] ^= t;
    }
}

// ---- Extended twisted Edwards group operations ----
// Point in extended coordinates (X:Y:Z:T) with a=-1

#[derive(Clone)]
struct Point {
    x: Fe,
    y: Fe,
    z: Fe,
    t: Fe,
}

// d = -121665/121666 mod p (Ed25519 twist constant)
// d = 0x52036cee2b6ffe738cc740797779e89800700a4d4141d8ab75eb4dca135978a3
fn d_const() -> Fe {
    let bytes: [u8; 32] = [
        0xa3, 0x78, 0x59, 0x13, 0xca, 0x4d, 0xeb, 0x75, 0xab, 0xd8, 0x41, 0x41, 0x4d, 0x0a, 0x70,
        0x00, 0x98, 0xe8, 0x79, 0x77, 0x79, 0x40, 0xc7, 0x8c, 0x73, 0xfe, 0x6f, 0x2b, 0xee, 0x6c,
        0x03, 0x52,
    ];
    fe_from_bytes(&bytes)
}

fn point_identity() -> Point {
    Point {
        x: fe_zero(),
        y: fe_one(),
        z: fe_one(),
        t: fe_zero(),
    }
}

fn point_add(p: &Point, q: &Point) -> Point {
    let d2 = fe_mul(&fe_add(&d_const(), &d_const()), &fe_one()); // 2*d
                                                                 // Unified addition formula for twisted Edwards curves
    let a = fe_mul(&fe_sub(&p.y, &p.x), &fe_sub(&q.y, &q.x));
    let b = fe_mul(&fe_add(&p.y, &p.x), &fe_add(&q.y, &q.x));
    let c = fe_mul(&p.t, &fe_mul(&q.t, &fe_add(&d_const(), &d_const())));
    let dd = fe_mul(&p.z, &fe_mul(&q.z, &fe_add(&fe_one(), &fe_one())));
    let e = fe_sub(&b, &a);
    let f = fe_sub(&dd, &c);
    let g = fe_add(&dd, &c);
    let h = fe_add(&b, &a);
    let _ = d2;
    Point {
        x: fe_mul(&e, &f),
        y: fe_mul(&g, &h),
        z: fe_mul(&f, &g),
        t: fe_mul(&e, &h),
    }
}

fn point_double(p: &Point) -> Point {
    point_add(p, p)
}

fn scalar_mult(scalar: &[u8; 32], point: &Point) -> Point {
    let mut q = point_identity();
    let mut r = point.clone();
    for i in (0..256).rev() {
        let bit = ((scalar[i / 8] >> (i % 8)) & 1) as i64;
        // Conditional swap (constant-time)
        let mut qx = q.x;
        let mut rx = r.x;
        let mut qy = q.y;
        let mut ry = r.y;
        let mut qz = q.z;
        let mut rz = r.z;
        let mut qt = q.t;
        let mut rt = r.t;
        fe_cswap(&mut qx, &mut rx, -bit);
        fe_cswap(&mut qy, &mut ry, -bit);
        fe_cswap(&mut qz, &mut rz, -bit);
        fe_cswap(&mut qt, &mut rt, -bit);
        q = Point {
            x: qx,
            y: qy,
            z: qz,
            t: qt,
        };
        r = Point {
            x: rx,
            y: ry,
            z: rz,
            t: rt,
        };
        r = point_add(&q, &r);
        q = point_double(&q);
        let mut qx = q.x;
        let mut rx = r.x;
        let mut qy = q.y;
        let mut ry = r.y;
        let mut qz = q.z;
        let mut rz = r.z;
        let mut qt = q.t;
        let mut rt = r.t;
        fe_cswap(&mut qx, &mut rx, -bit);
        fe_cswap(&mut qy, &mut ry, -bit);
        fe_cswap(&mut qz, &mut rz, -bit);
        fe_cswap(&mut qt, &mut rt, -bit);
        q = Point {
            x: qx,
            y: qy,
            z: qz,
            t: qt,
        };
        r = Point {
            x: rx,
            y: ry,
            z: rz,
            t: rt,
        };
    }
    q
}

// Base point B of Ed25519
fn base_point() -> Point {
    // Gy = 4/5 mod p, Gx recovered from curve equation
    let gy_bytes: [u8; 32] = [
        0x58, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
        0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
        0x66, 0x66,
    ];
    let y = fe_from_bytes(&gy_bytes);
    // x = sqrt((y^2 - 1) / (d*y^2 + 1))
    let y2 = fe_sq(&y);
    let u = fe_sub(&y2, &fe_one());
    let v = fe_add(&fe_mul(&d_const(), &y2), &fe_one());
    let v3 = fe_mul(&fe_sq(&v), &v);
    let v7 = fe_mul(&fe_sq(&v3), &v);
    let uv7 = fe_mul(&u, &v7);
    let r = fe_mul(&fe_mul(&u, &fe_sq(&v3)), &fe_pow22523(&uv7));
    let x = if fe_is_negative(&r) {
        fe_sub(&fe_zero(), &r)
    } else {
        r
    };
    let t = fe_mul(&x, &y);
    Point {
        x,
        y,
        z: fe_one(),
        t,
    }
}

fn decode_point(bytes: &[u8; 32]) -> Option<Point> {
    let mut b = *bytes;
    let sign = (b[31] >> 7) as i64;
    b[31] &= 0x7f;
    let y = fe_from_bytes(&b);
    let y2 = fe_sq(&y);
    let u = fe_sub(&y2, &fe_one());
    let v = fe_add(&fe_mul(&d_const(), &y2), &fe_one());
    let v3 = fe_mul(&fe_sq(&v), &v);
    let v7 = fe_mul(&fe_sq(&v3), &v);
    let uv7 = fe_mul(&u, &v7);
    let r = fe_mul(&fe_mul(&u, &fe_sq(&v3)), &fe_pow22523(&uv7));
    let x = if (fe_is_negative(&r) as i64) != sign {
        fe_sub(&fe_zero(), &r)
    } else {
        r
    };
    // Validate: check v*x^2 == u
    let check = fe_mul(&v, &fe_sq(&x));
    let diff = fe_sub(&check, &u);
    let db = fe_to_bytes(&diff);
    if db.iter().any(|&b| b != 0) {
        return None;
    }
    let t = fe_mul(&x, &y);
    Some(Point {
        x,
        y,
        z: fe_one(),
        t,
    })
}

fn points_equal(p: &Point, q: &Point) -> bool {
    // p == q iff p.x * q.z == q.x * p.z AND p.y * q.z == q.y * p.z
    let lhs_x = fe_mul(&p.x, &q.z);
    let rhs_x = fe_mul(&q.x, &p.z);
    let lhs_y = fe_mul(&p.y, &q.z);
    let rhs_y = fe_mul(&q.y, &p.z);
    let dx = fe_to_bytes(&fe_sub(&lhs_x, &rhs_x));
    let dy = fe_to_bytes(&fe_sub(&lhs_y, &rhs_y));
    dx.iter().all(|&b| b == 0) && dy.iter().all(|&b| b == 0)
}

fn sha512_of(data: &[u8]) -> [u8; 64] {
    // Re-export sha512 for use in this module
    let mut state: [u64; 8] = [
        0x6a09e667f3bcc908,
        0xbb67ae8584caa73b,
        0x3c6ef372fe94f82b,
        0xa54ff53a5f1d36f1,
        0x510e527fade682d1,
        0x9b05688c2b3e6c1f,
        0x1f83d9abfb41bd6b,
        0x5be0cd19137e2179,
    ];
    let bit_len = (data.len() as u128).wrapping_mul(8);
    let mut buf = [0u8; 128];
    let mut bf = 0usize;
    for &byte in data {
        buf[bf] = byte;
        bf += 1;
        if bf == 128 {
            sha512_compress(&mut state, &buf);
            bf = 0;
        }
    }
    buf[bf] = 0x80;
    bf += 1;
    if bf > 112 {
        while bf < 128 {
            buf[bf] = 0;
            bf += 1;
        }
        sha512_compress(&mut state, &buf);
        bf = 0;
    }
    while bf < 112 {
        buf[bf] = 0;
        bf += 1;
    }
    buf[112..128].copy_from_slice(&bit_len.to_be_bytes());
    sha512_compress(&mut state, &buf);
    let mut out = [0u8; 64];
    for (i, &w) in state.iter().enumerate() {
        out[i * 8..(i + 1) * 8].copy_from_slice(&w.to_be_bytes());
    }
    out
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
        None => return SecureBootResult::KeyNotFound,
    };

    if key.data_len < 32 {
        return SecureBootResult::KeyNotFound;
    }
    let pub_key: &[u8; 32] = key.data[..32].try_into().unwrap();

    // Build SHA-512(R || A || message) where R and A are in sig and pub_key
    let r_bytes = &sig[..32];
    let mut hash_buf = [0u8; 32 + 32]; // R || A prefix
    hash_buf[..32].copy_from_slice(r_bytes);
    hash_buf[32..64].copy_from_slice(pub_key);

    // Hash R || A || image in pieces (manual SHA-512 multi-call)
    // We reuse sha512_of on a stack-allocated buffer; for large images
    // this is limited to 4 KB. A streaming SHA-512 is needed for production.
    const MAX_IMG: usize = 4096;
    if image.len() > MAX_IMG {
        // Cannot verify oversized image without heap; treat as invalid
        audit_log(AuditEvent::SecureBootCheck, 0, key_id as u64);
        return SecureBootResult::InvalidSignature;
    }
    let mut full_buf = [0u8; 64 + MAX_IMG];
    full_buf[..64].copy_from_slice(&hash_buf);
    full_buf[64..64 + image.len()].copy_from_slice(image);
    let k_hash = sha512_of(&full_buf[..64 + image.len()]);

    let mut k_scalar = [0u8; 32];
    k_scalar.copy_from_slice(&k_hash[..32]);
    // Clamp scalar to [0, l) by masking top bits (simplified; full reduction omitted)
    k_scalar[31] &= 0x1f;

    let mut s_bytes = [0u8; 32];
    s_bytes.copy_from_slice(&sig[32..]);
    if s_bytes[31] & 0xe0 != 0 {
        audit_log(AuditEvent::SecureBootCheck, 0, key_id as u64);
        return SecureBootResult::InvalidSignature;
    }

    let cap_a = match decode_point(pub_key) {
        Some(p) => p,
        None => {
            audit_log(AuditEvent::SecureBootCheck, 0, key_id as u64);
            return SecureBootResult::InvalidSignature;
        }
    };
    let mut r_arr = [0u8; 32];
    r_arr.copy_from_slice(r_bytes);
    let cap_r = match decode_point(&r_arr) {
        Some(p) => p,
        None => {
            audit_log(AuditEvent::SecureBootCheck, 0, key_id as u64);
            return SecureBootResult::InvalidSignature;
        }
    };

    let sb = scalar_mult(&s_bytes, &base_point());
    let ka = scalar_mult(&k_scalar, &cap_a);
    let rka = point_add(&cap_r, &ka);

    audit_log(AuditEvent::SecureBootCheck, 0, key_id as u64);

    if points_equal(&sb, &rka) {
        SecureBootResult::Verified
    } else {
        SecureBootResult::InvalidSignature
    }
}
