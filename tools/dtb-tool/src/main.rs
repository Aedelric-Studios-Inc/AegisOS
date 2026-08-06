//! AegisOS DTB tool — compile/decompile device tree blobs.
use std::env;
fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 3 {
        eprintln!("Usage: dtb-tool <compile|decompile> <file>");
        std::process::exit(1);
    }
    println!("[dtb-tool] {} {}", args[1], args[2]);
}
