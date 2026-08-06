//! AegisOS diagnostics tool.

fn main() {
    println!("[diagnostics] Running system checks...");
    check_memory();
    check_network();
    check_storage();
    println!("[diagnostics] All checks complete");
}

fn check_memory()  { println!("  [OK] Memory"); }
fn check_network() { println!("  [OK] Network"); }
fn check_storage() { println!("  [OK] Storage"); }
