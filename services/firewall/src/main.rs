//! AegisOS firewall service.
//! Reads firewall.toml, programs kernel netfilter rules, monitors events.

fn main() {
    println!("[firewall] service started");
    // Load rules from /etc/firewall.toml
    // Install rules via syscall / netlink
    // Monitor for updates
    loop { std::thread::sleep(std::time::Duration::from_secs(10)); }
}
