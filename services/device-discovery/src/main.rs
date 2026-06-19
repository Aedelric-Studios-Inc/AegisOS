//! AegisOS LAN device discovery (mDNS / ARP scan).
fn main() {
    println!("[device-discovery] service started");
    // Periodic ARP sweep, publish mDNS — TODO
    loop { std::thread::sleep(std::time::Duration::from_secs(30)); }
}
