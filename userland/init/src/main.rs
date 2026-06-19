//! AegisOS init — PID 1.
//! Mounts filesystems, starts services, supervises the system.

fn main() {
    eprintln!("[init] AegisOS init starting");

    // Mount essential filesystems
    mount_filesystems();

    // Start core services
    start_services();

    // Enter supervision loop
    supervision_loop();
}

fn mount_filesystems() {
    // Mount /proc, /sys, /dev, /run — TODO: syscall wrappers
}

fn start_services() {
    // Launch service-manager, firewall, etc. — TODO
}

fn supervision_loop() -> ! {
    loop {
        // Wait for child exit signals, restart as needed — TODO
        unsafe { core::arch::asm!("wfe"); }
    }
}
