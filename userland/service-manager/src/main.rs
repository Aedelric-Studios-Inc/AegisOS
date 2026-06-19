//! AegisOS service manager.

fn main() {
    println!("[service-manager] started");
    // Read /etc/services.toml, fork & exec services — TODO
    loop {
        std::thread::sleep(std::time::Duration::from_secs(1));
    }
}
