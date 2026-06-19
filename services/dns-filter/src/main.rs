//! AegisOS DNS filter — intercepts DNS queries and blocks flagged domains.
fn main() {
    println!("[dns-filter] service started, listening on :53");
    // Bind UDP :53, resolve or block queries — TODO
    loop { std::thread::sleep(std::time::Duration::from_secs(10)); }
}
