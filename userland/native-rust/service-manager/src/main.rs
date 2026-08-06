#![no_std]
#![no_main]
use aegis_rt::*;

const HEALTH: &[u8] = b"aegisd-ready";
const AEGISD: &[u8] = b"/sbin/aegisd\0";
const DASHBOARD: &[u8] = b"/sbin/dashboard\0";
const RUSTMYADMIN: &[u8] = b"/opt/aegisos/rustmyadmin\0";

fn spawn_bounded(path: &'static [u8]) -> i64 {
    let mut delay = 10u64;
    for _ in 0..6 {
        let pid = spawn(path);
        if pid > 0 { return pid; }
        let _ = sleep_ms(delay);
        delay = core::cmp::min(delay.saturating_mul(2), 5000);
    }
    -1
}

fn restart_child(path: &'static [u8], failures: &mut u32) -> i64 {
    *failures += 1;
    if *failures > 5 { return -1; }
    spawn_bounded(path)
}

#[no_mangle]
pub extern "C" fn _start() -> ! {
    log(b"[service-manager:rust] native Rust supervisor online\n");
    if service_ready(b"service-manager\0") < 0 { exit(1); }
    let channel = channel_open(b"aegisd-health\0");
    if channel <= 0 { exit(2); }

    let mut aegisd = spawn_bounded(AEGISD);
    if aegisd < 0 { exit(3); }
    let mut dashboard = -1i64;
    let mut rustmyadmin = -1i64;
    let mut aegisd_failures = 0u32;
    let mut dashboard_failures = 0u32;
    let mut rustmyadmin_failures = 0u32;
    let mut buf = [0u8; 64];

    loop {
        let got = recv_msg(channel, &mut buf);
        if got == HEALTH.len() as i64 && &buf[..HEALTH.len()] == HEALTH {
            aegisd_failures = 0;
            log(b"[service-manager:rust] aegisd IPC health confirmed\n");
            if dashboard < 0 { dashboard = spawn_bounded(DASHBOARD); }
            if rustmyadmin < 0 { rustmyadmin = spawn_bounded(RUSTMYADMIN); }
            if dashboard > 0 && rustmyadmin > 0 { let _ = console_ready(); }
        }

        let mut status = 0;
        if waitpid(aegisd, &mut status, WAIT_NOHANG) == aegisd {
            log(b"[service-manager:rust] reaped aegisd; applying restart policy\n");
            aegisd = restart_child(AEGISD, &mut aegisd_failures);
            if aegisd < 0 { exit(70); }
        }
        if dashboard > 0 && waitpid(dashboard, &mut status, WAIT_NOHANG) == dashboard {
            log(b"[service-manager:rust] reaped dashboard; applying restart policy\n");
            dashboard = restart_child(DASHBOARD, &mut dashboard_failures);
            if dashboard < 0 { exit(71); }
        }
        if rustmyadmin > 0 && waitpid(rustmyadmin, &mut status, WAIT_NOHANG) == rustmyadmin {
            log(b"[service-manager:rust] reaped RustMyAdmin; applying restart policy\n");
            rustmyadmin = restart_child(RUSTMYADMIN, &mut rustmyadmin_failures);
            if rustmyadmin < 0 { exit(72); }
        }
        yield_now();
    }
}
