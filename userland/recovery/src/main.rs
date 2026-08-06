//! AegisOS recovery environment.
//! Interactive menu for factory reset, image flashing, and reboot.

use std::io::{self, BufRead, Write};

fn main() {
    println!("[recovery] AegisOS recovery mode");
    run_menu();
}

fn run_menu() {
    let stdin = io::stdin();
    loop {
        println!();
        println!("Recovery options:");
        println!("  1. Restore factory defaults");
        println!("  2. Flash new image");
        println!("  3. Reboot");
        println!("  4. Drop to shell");
        print!("Select option: ");
        io::stdout().flush().ok();

        let mut line = String::new();
        match stdin.lock().read_line(&mut line) {
            Ok(0) | Err(_) => {
                eprintln!("[recovery] stdin closed, rebooting");
                do_reboot();
            }
            Ok(_) => {}
        }

        match line.trim() {
            "1" => restore_factory_defaults(),
            "2" => flash_new_image(),
            "3" => do_reboot(),
            "4" => drop_to_shell(),
            other => eprintln!("[recovery] unknown option: {}", other),
        }
    }
}

fn restore_factory_defaults() {
    println!("[recovery] Restoring factory defaults...");
    // Remove user configuration overlay and reset to defaults
    let paths = ["/etc/config", "/etc/services.toml", "/var/db"];
    for path in &paths {
        match std::fs::remove_dir_all(path) {
            Ok(_) => println!("[recovery]   removed {}", path),
            Err(e) if e.kind() == std::io::ErrorKind::NotFound => {}
            Err(e) => eprintln!("[recovery]   {}: {}", path, e),
        }
    }
    println!("[recovery] Factory defaults restored. Rebooting...");
    do_reboot();
}

fn flash_new_image() {
    println!("[recovery] Flash new image");
    print!("Enter path to image file: ");
    io::stdout().flush().ok();
    let mut path = String::new();
    io::stdin().lock().read_line(&mut path).ok();
    let path = path.trim();
    if path.is_empty() {
        eprintln!("[recovery] no path given, aborting");
        return;
    }
    match std::fs::metadata(path) {
        Ok(m) => println!("[recovery] image size: {} bytes", m.len()),
        Err(e) => { eprintln!("[recovery] cannot open image: {}", e); return; }
    }
    // Write image to the inactive partition via /dev/mmcblk0p2
    let target = "/dev/mmcblk0p2";
    match std::process::Command::new("dd")
        .args(["if", path, "of", target, "bs=4M", "conv=fsync"])
        .status()
    {
        Ok(s) if s.success() => println!("[recovery] image flashed successfully"),
        Ok(s) => eprintln!("[recovery] dd exited with {}", s),
        Err(e) => eprintln!("[recovery] dd failed: {}", e),
    }
}

fn do_reboot() -> ! {
    println!("[recovery] Rebooting system...");
    let _ = std::process::Command::new("reboot").status();
    // Fallback: write to sysrq
    let _ = std::fs::write("/proc/sysrq-trigger", "b");
    std::process::exit(0);
}

fn drop_to_shell() {
    println!("[recovery] Dropping to shell (type 'exit' to return)");
    let shell = std::env::var("SHELL").unwrap_or_else(|_| "/bin/sh".to_owned());
    match std::process::Command::new(&shell).status() {
        Ok(_) => {}
        Err(e) => eprintln!("[recovery] shell error: {}", e),
    }
}
