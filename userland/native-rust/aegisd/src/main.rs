#![no_std]
#![no_main]
use aegis_rt::*;

#[no_mangle]
pub extern "C" fn _start() -> ! {
    log(b"[aegisd:rust] native Rust daemon online\n");
    if service_ready(b"aegisd\0") < 0 { exit(1); }
    let channel = channel_open(b"aegisd-health\0");
    if channel <= 0 || send_msg(channel, b"aegisd-ready") < 0 { exit(2); }
    let mut seed = [0u8; 32];
    let _ = random(&mut seed, false);
    loop { yield_now(); }
}
