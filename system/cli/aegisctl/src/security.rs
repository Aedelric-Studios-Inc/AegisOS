//! Security Operations commands for aegisctl.

use crate::{daemon_get, daemon_post};

pub fn run(args: &[String]) {
    let sub = args.first().map(String::as_str).unwrap_or("baseline");
    match sub {
        "baseline" | "status" => print_get("/security/baseline", "aegisctl security baseline"),
        "audit" => print_get("/security/audit", "aegisctl security audit"),
        "incidents" => print_get("/security/incidents", "aegisctl security incidents"),
        "enforce" => print_post(
            "/security/baseline/enforce",
            "",
            "aegisctl security enforce",
        ),
        _ => {
            eprintln!("aegisctl security: unknown sub-command '{}'", sub);
            eprintln!("  sub-commands: baseline, audit, incidents, enforce");
            std::process::exit(1);
        }
    }
}

fn print_get(path: &str, label: &str) {
    match daemon_get(path) {
        Ok(json) => println!("{}", json),
        Err(e) => {
            eprintln!("{}: {}", label, e);
            std::process::exit(1);
        }
    }
}

fn print_post(path: &str, body: &str, label: &str) {
    match daemon_post(path, body) {
        Ok(json) => println!("{}", json),
        Err(e) => {
            eprintln!("{}: {}", label, e);
            std::process::exit(1);
        }
    }
}
