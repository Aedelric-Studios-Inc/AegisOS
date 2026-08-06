//! Network/router commands for aegisctl.

use crate::{daemon_get, daemon_post};

pub fn run(args: &[String]) {
    let sub = args.first().map(String::as_str).unwrap_or("interfaces");
    match sub {
        "interfaces" | "status" => print_get("/network/interfaces", "aegisctl network interfaces"),
        "apply" => print_post("/network/apply", "", "aegisctl network apply"),
        _ => {
            eprintln!("aegisctl network: unknown sub-command '{}'", sub);
            eprintln!("  sub-commands: interfaces, apply");
            std::process::exit(1);
        }
    }
}

pub fn router(args: &[String]) {
    let sub = args.first().map(String::as_str).unwrap_or("status");
    match sub {
        "status" => print_get("/router/status", "aegisctl router status"),
        "provision" => print_post("/router/provision", "", "aegisctl router provision"),
        _ => {
            eprintln!("aegisctl router: unknown sub-command '{}'", sub);
            eprintln!("  sub-commands: status, provision");
            std::process::exit(1);
        }
    }
}

pub fn radio(args: &[String]) {
    let sub = args.first().map(String::as_str).unwrap_or("status");
    match sub {
        "status" => print_get("/radio/status", "aegisctl radio status"),
        "connect" => print_post("/radio/connect", "", "aegisctl radio connect"),
        "disconnect" => print_post("/radio/disconnect", "", "aegisctl radio disconnect"),
        _ => {
            eprintln!("aegisctl radio: unknown sub-command '{}'", sub);
            eprintln!("  sub-commands: status, connect, disconnect");
            std::process::exit(1);
        }
    }
}

pub fn dns(args: &[String]) {
    let sub = args.first().map(String::as_str).unwrap_or("status");
    match sub {
        "status" => print_get("/dns/status", "aegisctl dns status"),
        "reload" => print_post("/dns/reload", "", "aegisctl dns reload"),
        _ => {
            eprintln!("aegisctl dns: unknown sub-command '{}'", sub);
            eprintln!("  sub-commands: status, reload");
            std::process::exit(1);
        }
    }
}

fn print_get(path: &str, label: &str) {
    match daemon_get(path) {
        Ok(json) => println!("{}", json),
        Err(e) => { eprintln!("{}: {}", label, e); std::process::exit(1); }
    }
}

fn print_post(path: &str, body: &str, label: &str) {
    match daemon_post(path, body) {
        Ok(json) => println!("{}", json),
        Err(e) => { eprintln!("{}: {}", label, e); std::process::exit(1); }
    }
}
