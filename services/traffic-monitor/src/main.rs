//! AegisOS traffic monitor — captures flow statistics.
fn main() {
    println!("[traffic-monitor] service started");
    // Capture packets, aggregate per-flow stats — TODO
    loop { std::thread::sleep(std::time::Duration::from_secs(5)); }
}
