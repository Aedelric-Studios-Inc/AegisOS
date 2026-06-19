//! AegisOS init — PID 1.
//! Mounts filesystems, starts services, supervises the system.

use std::process::{Child, Command};
use std::collections::HashMap;
use std::time::Duration;

fn main() {
    eprintln!("[init] AegisOS init starting");

    mount_filesystems();
    start_services();
    supervision_loop();
}

fn mount_filesystems() {
    // Mount essential pseudo-filesystems via the mount(8) utility.
    // On a live AegisOS system these would be issued as direct syscalls.
    let mounts = [
        ("proc",    "/proc", "proc"),
        ("sysfs",   "/sys",  "sysfs"),
        ("devtmpfs","/dev",  "devtmpfs"),
        ("tmpfs",   "/run",  "tmpfs"),
    ];
    for (fstype, target, src) in &mounts {
        match std::fs::create_dir_all(target) {
            Ok(_) => {}
            Err(e) => eprintln!("[init] mkdir {}: {}", target, e),
        }
        match Command::new("mount").args(["-t", fstype, src, target]).status() {
            Ok(s) if s.success() => eprintln!("[init] mounted {} on {}", fstype, target),
            Ok(s) => eprintln!("[init] mount {} failed: {}", target, s),
            Err(e) => eprintln!("[init] mount {}: {}", target, e),
        }
    }
}

/// Core services spawned by init.  Each entry is (name, binary).
const CORE_SERVICES: &[(&str, &str)] = &[
    ("service-manager", "/sbin/service-manager"),
    ("firewall",        "/sbin/firewall"),
];

fn start_services() -> HashMap<String, Child> {
    let mut children: HashMap<String, Child> = HashMap::new();
    for &(name, bin) in CORE_SERVICES {
        match Command::new(bin).spawn() {
            Ok(child) => {
                eprintln!("[init] spawned {} (pid {})", name, child.id());
                children.insert(name.to_owned(), child);
            }
            Err(e) => eprintln!("[init] failed to spawn {}: {}", name, e),
        }
    }
    children
}

fn supervision_loop() -> ! {
    let mut children = start_services();
    loop {
        // Reap and restart any exited children.
        for &(name, bin) in CORE_SERVICES {
            let restart = match children.get_mut(name) {
                Some(child) => matches!(child.try_wait(), Ok(Some(_))),
                None => true,
            };
            if restart {
                eprintln!("[init] restarting {}", name);
                match Command::new(bin).spawn() {
                    Ok(child) => { children.insert(name.to_owned(), child); }
                    Err(e) => eprintln!("[init] restart {}: {}", name, e),
                }
            }
        }
        std::thread::sleep(Duration::from_secs(1));
    }
}
