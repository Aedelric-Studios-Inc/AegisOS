//! aegisctl — AegisOS management CLI.

use std::env;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        eprintln!("Usage: aegisctl <command> [args...]");
        eprintln!("Commands: status, firewall, vpn, update, reboot, poweroff");
        std::process::exit(1);
    }
    match args[1].as_str() {
        "status"   => cmd_status(),
        "reboot"   => cmd_reboot(),
        "poweroff" => cmd_poweroff(),
        cmd => eprintln!("Unknown command: {cmd}"),
    }
}

fn cmd_status()   { println!("AegisOS status: OK"); }
fn cmd_reboot()   { println!("Rebooting..."); }
fn cmd_poweroff() { println!("Powering off..."); }
