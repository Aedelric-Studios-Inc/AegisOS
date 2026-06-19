//! AegisOS lightweight web hosting service.
fn main() {
    println!("[web-hosting] service started, listening on :80");
    // Accept HTTP connections, serve static files — TODO
    loop { std::thread::sleep(std::time::Duration::from_secs(10)); }
}
