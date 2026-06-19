//! AegisOS interactive shell.

use std::io::{self, BufRead, Write};

fn main() {
    let stdin = io::stdin();
    print!("aegis> ");
    io::stdout().flush().ok();
    for line in stdin.lock().lines() {
        let line = line.expect("read error");
        dispatch(&line);
        print!("aegis> ");
        io::stdout().flush().ok();
    }
}

fn dispatch(cmd: &str) {
    let parts: Vec<&str> = cmd.split_whitespace().collect();
    match parts.first().copied() {
        Some("help") => println!("Commands: help, exit, echo, ls"),
        Some("echo") => println!("{}", parts[1..].join(" ")),
        Some("exit") => std::process::exit(0),
        Some(other)  => eprintln!("Unknown: {other}"),
        None => {}
    }
}
