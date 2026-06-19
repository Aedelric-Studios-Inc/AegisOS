//! AegisOS image signing tool.
//!
//! Signs a boot image with an Ed25519 private key and appends the 64-byte
//! signature to the output file.
//!
//! Usage:
//!   sign-image <image> <key.pem> [output]
//!   sign-image --verify <image> <pubkey.pem>
//!
//! The private key must be a PKCS#8-encoded Ed25519 key in PEM format
//! (as produced by `openssl genpkey -algorithm ed25519 -out key.pem`).
//! For testing, a raw 32-byte hex seed (64 hex characters) is also accepted.
//!
//! The output file is the image with the 64-byte Ed25519 signature appended.
//! If no output path is given the image is signed in-place.

use std::env;
use std::fs;
use std::io::{self, Write};

use ed25519_dalek::{Signer, SigningKey, Verifier, VerifyingKey};

fn main() {
    let args: Vec<String> = env::args().collect();

    // --verify mode: sign-image --verify <image> <pubkey.pem>
    if args.get(1).map(String::as_str) == Some("--verify") {
        if args.len() < 4 {
            eprintln!("Usage: sign-image --verify <image> <pubkey.pem>");
            std::process::exit(1);
        }
        return cmd_verify(&args[2], &args[3]);
    }

    if args.len() < 3 {
        eprintln!("Usage: sign-image <image> <key.pem> [output]");
        eprintln!("       sign-image --verify <image> <pubkey.pem>");
        std::process::exit(1);
    }

    let image_path = &args[1];
    let key_path   = &args[2];
    let out_path   = args.get(3).map(String::as_str)
        .unwrap_or(image_path.as_str()); // sign in-place by default

    println!("[sign-image] Signing {} with {}", image_path, key_path);

    let image = read_file(image_path);
    let key_pem = read_text(key_path);

    let signing_key = match parse_signing_key(&key_pem) {
        Some(k) => k,
        None => {
            eprintln!("[sign-image] could not parse Ed25519 private key from {}", key_path);
            eprintln!("[sign-image] accepted formats:");
            eprintln!("  • PKCS#8 PEM  (openssl genpkey -algorithm ed25519 -out key.pem)");
            eprintln!("  • Raw 64-char hex seed (for testing only)");
            std::process::exit(1);
        }
    };

    let sig = signing_key.sign(&image);
    let sig_bytes: [u8; 64] = sig.to_bytes();

    let tmp = format!("{}.tmp", out_path);
    if let Err(e) = write_signed(&tmp, &image, &sig_bytes) {
        eprintln!("[sign-image] write {}: {}", tmp, e);
        std::process::exit(1);
    }
    if let Err(e) = fs::rename(&tmp, out_path) {
        eprintln!("[sign-image] rename to {}: {}", out_path, e);
        let _ = fs::remove_file(&tmp);
        std::process::exit(1);
    }

    let verifying_key = signing_key.verifying_key();
    println!("[sign-image] signed successfully ({} bytes total)", image.len() + 64);
    println!("[sign-image] public key (hex): {}", hex_encode(verifying_key.as_bytes()));
}

fn cmd_verify(image_path: &str, pubkey_path: &str) {
    println!("[sign-image] Verifying {} with {}", image_path, pubkey_path);

    let signed = read_file(image_path);
    if signed.len() < 64 {
        eprintln!("[sign-image] file too small to contain a signature ({} bytes)", signed.len());
        std::process::exit(1);
    }

    let (image, sig_bytes) = signed.split_at(signed.len() - 64);
    let sig_arr: [u8; 64] = sig_bytes.try_into().expect("slice is exactly 64 bytes");
    let sig = match ed25519_dalek::Signature::from_bytes(&sig_arr).try_into() {
        Ok(s) => s,
        Err(_) => {
            let s: ed25519_dalek::Signature = ed25519_dalek::Signature::from_bytes(&sig_arr);
            s
        }
    };

    let key_pem = read_text(pubkey_path);
    let verifying_key = match parse_verifying_key(&key_pem) {
        Some(k) => k,
        None => {
            eprintln!("[sign-image] could not parse Ed25519 public key from {}", pubkey_path);
            std::process::exit(1);
        }
    };

    match verifying_key.verify(image, &sig) {
        Ok(()) => println!("[sign-image] signature VALID"),
        Err(e) => {
            eprintln!("[sign-image] signature INVALID: {}", e);
            std::process::exit(2);
        }
    }
}

// ---- Key parsing helpers ----

/// Parse an Ed25519 signing key.
/// Accepts PKCS#8 PEM or a raw 64-hex-character seed (for testing).
fn parse_signing_key(pem: &str) -> Option<SigningKey> {
    use ed25519_dalek::pkcs8::DecodePrivateKey;

    // Try PKCS#8 PEM first.
    if let Ok(key) = SigningKey::from_pkcs8_pem(pem) {
        return Some(key);
    }

    // Fall back to raw 32-byte hex seed (testing/CI usage).
    let hex: String = pem.trim().chars().filter(|c| c.is_ascii_hexdigit()).collect();
    if hex.len() == 64 {
        let seed = decode_hex32(&hex)?;
        return Some(SigningKey::from_bytes(&seed));
    }

    None
}

/// Parse an Ed25519 verifying (public) key from PEM or 64-hex-char encoding.
fn parse_verifying_key(pem: &str) -> Option<VerifyingKey> {
    use ed25519_dalek::pkcs8::DecodePublicKey;

    if let Ok(key) = VerifyingKey::from_public_key_pem(pem) {
        return Some(key);
    }

    // Raw 64-hex-char public key bytes.
    let hex: String = pem.trim().chars().filter(|c| c.is_ascii_hexdigit()).collect();
    if hex.len() == 64 {
        let bytes = decode_hex32(&hex)?;
        return VerifyingKey::from_bytes(&bytes).ok();
    }

    None
}

fn decode_hex32(hex: &str) -> Option<[u8; 32]> {
    let mut out = [0u8; 32];
    for (i, chunk) in hex.as_bytes().chunks(2).enumerate() {
        out[i] = u8::from_str_radix(std::str::from_utf8(chunk).ok()?, 16).ok()?;
    }
    Some(out)
}

fn hex_encode(bytes: &[u8]) -> String {
    bytes.iter().map(|b| format!("{:02x}", b)).collect()
}

// ---- I/O helpers ----

fn read_file(path: &str) -> Vec<u8> {
    match fs::read(path) {
        Ok(b) => b,
        Err(e) => { eprintln!("[sign-image] read {}: {}", path, e); std::process::exit(1); }
    }
}

fn read_text(path: &str) -> String {
    match fs::read_to_string(path) {
        Ok(s) => s,
        Err(e) => { eprintln!("[sign-image] read {}: {}", path, e); std::process::exit(1); }
    }
}

fn write_signed(path: &str, image: &[u8], sig: &[u8; 64]) -> io::Result<()> {
    let mut f = fs::File::create(path)?;
    f.write_all(image)?;
    f.write_all(sig)?;
    f.flush()?;
    Ok(())
}
