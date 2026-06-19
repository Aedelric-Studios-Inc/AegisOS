//! AegisOS container runner — lightweight OCI-compatible container runtime.
fn main() {
    println!("[container-runner] service started");
    // Pull image, set up namespace/cgroups, exec — TODO
    loop { std::thread::sleep(std::time::Duration::from_secs(10)); }
}
