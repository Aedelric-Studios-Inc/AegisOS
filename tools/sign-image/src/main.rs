//! AegisOS image signing tool.
use std::env;
fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 3 {
        eprintln!("Usage: sign-image <image> <key.pem>");
        std::process::exit(1);
    }
    println!("[sign-image] Signing {} with {}", args[1], args[2]);
    // Ed25519 sign image, append signature — TODO
}
