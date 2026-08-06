#![no_std]
#![no_main]
use aegis_rt::*;

const MANAGER: &[u8] = b"/sbin/service-manager\0";

fn spawn_manager() -> i64 {
    let mut delay = 10u64;
    for _ in 0..6 {
        let pid = spawn(MANAGER);
        if pid > 0 { return pid; }
        let _ = sleep_ms(delay);
        delay = core::cmp::min(delay.saturating_mul(2), 2000);
    }
    -1
}

#[no_mangle]
pub extern "C" fn _start() -> ! {
    log(b"[aegis-init:rust] native Rust PID 1 online under AegisOS\n");
    if getpid() != 1 || service_ready(b"aegis-init\0") < 0 { exit(1); }
    let mut manager = spawn_manager();
    if manager < 0 { exit(2); }
    log(b"[aegis-init:rust] service-manager started\n");

    let mut consecutive_failures = 0u32;
    loop {
        let mut status = 0;
        let reaped = waitpid(manager, &mut status, WAIT_NOHANG);
        if reaped == manager {
            consecutive_failures += 1;
            log(b"[aegis-init:rust] service-manager exited; restarting control plane\n");
            if consecutive_failures > 5 {
                log(b"[aegis-init:rust] service-manager restart budget exhausted\n");
                let _ = shutdown(0, 0x4d475246); /* MGRF */
                exit(70);
            }
            manager = spawn_manager();
            if manager < 0 { exit(71); }
        }
        yield_now();
    }
}
