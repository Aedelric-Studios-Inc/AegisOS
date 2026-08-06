//! AegisOS network control CLI.

use std::env;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        eprintln!("Usage: netctl <up|down|status|config>");
        std::process::exit(1);
    }
    match args[1].as_str() {
        "status" => println!("Network: up"),
        "up"     => println!("Bringing network up..."),
        "down"   => println!("Bringing network down..."),
        other    => eprintln!("Unknown subcommand: {other}"),
    }
}
