#![no_std]

use core::alloc::{GlobalAlloc, Layout};
use core::arch::asm;
use core::panic::PanicInfo;
use core::sync::atomic::{AtomicUsize, Ordering};

pub const SYS_READ: u64 = 0;
pub const SYS_OPEN: u64 = 2;
pub const SYS_CLOSE: u64 = 3;
pub const SYS_WRITE: u64 = 1;
pub const SYS_EXIT: u64 = 4;
pub const SYS_GETPID: u64 = 5;
pub const SYS_SEND_MSG: u64 = 8;
pub const SYS_RECV_MSG: u64 = 9;
pub const SYS_YIELD: u64 = 12;
pub const SYS_SPAWN: u64 = 13;
pub const SYS_WAITPID: u64 = 14;
pub const SYS_CHANNEL_OPEN: u64 = 18;
pub const SYS_SERVICE_READY: u64 = 19;
pub const SYS_CONSOLE_READY: u64 = 20;
pub const SYS_KILL: u64 = 21;
pub const SYS_SLEEP: u64 = 22;
pub const SYS_CLOCK_GET: u64 = 23;
pub const SYS_SHUTDOWN: u64 = 24;
pub const SYS_SOCKET: u64 = 25;
pub const SYS_BIND: u64 = 26;
pub const SYS_LISTEN: u64 = 27;
pub const SYS_ACCEPT: u64 = 28;
pub const SYS_CONNECT: u64 = 29;
pub const SYS_SEND: u64 = 30;
pub const SYS_RECV: u64 = 31;
pub const SYS_RANDOM: u64 = 32;

pub const EAGAIN: i64 = -8;
pub const WAIT_NOHANG: u64 = 1;

pub const SOCK_STREAM: u64 = 1;
pub const SOCK_DGRAM: u64 = 2;
pub const CLOCK_MONOTONIC: u64 = 1;
pub const CLOCK_REALTIME: u64 = 2;
#[repr(C)] pub struct Timespec { pub seconds: u64, pub nanoseconds: u64 }

pub fn open(path: &'static [u8], flags: u64) -> i64 { syscall3(SYS_OPEN,path.as_ptr() as u64,flags,0) }
pub fn close(fd: i64) -> i64 { syscall1(SYS_CLOSE,fd as u64) }
pub fn read(fd: i64, bytes: &mut [u8]) -> i64 { syscall3(SYS_READ,fd as u64,bytes.as_mut_ptr() as u64,bytes.len() as u64) }
pub fn socket(kind:u64)->i64{syscall1(SYS_SOCKET,kind)}
pub fn bind(fd:i64,port:u16)->i64{syscall2(SYS_BIND,fd as u64,port as u64)}
pub fn listen(fd:i64)->i64{syscall1(SYS_LISTEN,fd as u64)}
pub fn accept(fd:i64)->i64{syscall1(SYS_ACCEPT,fd as u64)}
pub fn connect(fd:i64,ip:u32,port:u16)->i64{syscall3(SYS_CONNECT,fd as u64,ip as u64,port as u64)}
pub fn send(fd:i64,data:&[u8])->i64{syscall3(SYS_SEND,fd as u64,data.as_ptr() as u64,data.len() as u64)}
pub fn recv(fd:i64,data:&mut[u8])->i64{syscall3(SYS_RECV,fd as u64,data.as_mut_ptr() as u64,data.len() as u64)}
pub fn clock_get(id:u64,out:&mut Timespec)->i64{syscall2(SYS_CLOCK_GET,id,out as *mut Timespec as u64)}
pub fn shutdown(mode:u64,reason:u64)->i64{syscall2(SYS_SHUTDOWN,mode,reason)}

#[inline(always)]
pub unsafe fn syscall6(nr: u64, a0: u64, a1: u64, a2: u64, a3: u64, a4: u64, a5: u64) -> i64 {
    let mut x0 = a0;
    asm!(
        "svc #0",
        in("x8") nr,
        inout("x0") x0,
        in("x1") a1,
        in("x2") a2,
        in("x3") a3,
        in("x4") a4,
        in("x5") a5,
        options(nostack)
    );
    x0 as i64
}

#[inline(always)] pub fn syscall0(n: u64) -> i64 { unsafe { syscall6(n,0,0,0,0,0,0) } }
#[inline(always)] pub fn syscall1(n: u64,a: u64) -> i64 { unsafe { syscall6(n,a,0,0,0,0,0) } }
#[inline(always)] pub fn syscall2(n: u64,a: u64,b: u64) -> i64 { unsafe { syscall6(n,a,b,0,0,0,0) } }
#[inline(always)] pub fn syscall3(n: u64,a: u64,b: u64,c: u64) -> i64 { unsafe { syscall6(n,a,b,c,0,0,0) } }

pub fn write(fd: u64, bytes: &[u8]) -> i64 { syscall3(SYS_WRITE, fd, bytes.as_ptr() as u64, bytes.len() as u64) }
pub fn log(bytes: &[u8]) { let _ = write(1, bytes); }
pub fn getpid() -> i64 { syscall0(SYS_GETPID) }
pub fn yield_now() { let _ = syscall0(SYS_YIELD); }
pub fn sleep_ms(ms: u64) -> i64 { syscall1(SYS_SLEEP, ms) }
pub fn spawn(path: &'static [u8]) -> i64 { syscall2(SYS_SPAWN, path.as_ptr() as u64, 0) }
pub fn waitpid(pid: i64, code: &mut i32, flags: u64) -> i64 { syscall3(SYS_WAITPID, pid as u64, code as *mut i32 as u64, flags) }
pub fn service_ready(name: &'static [u8]) -> i64 { syscall1(SYS_SERVICE_READY, name.as_ptr() as u64) }
pub fn channel_open(name: &'static [u8]) -> i64 { syscall1(SYS_CHANNEL_OPEN, name.as_ptr() as u64) }
pub fn send_msg(channel: i64, bytes: &[u8]) -> i64 { syscall3(SYS_SEND_MSG, channel as u64, bytes.as_ptr() as u64, bytes.len() as u64) }
pub fn recv_msg(channel: i64, bytes: &mut [u8]) -> i64 { syscall3(SYS_RECV_MSG, channel as u64, bytes.as_mut_ptr() as u64, bytes.len() as u64) }
pub fn console_ready() -> i64 { syscall0(SYS_CONSOLE_READY) }
pub fn random(bytes: &mut [u8], strong: bool) -> i64 { syscall3(SYS_RANDOM, bytes.as_mut_ptr() as u64, bytes.len() as u64, strong as u64) }
pub fn exit(code: i32) -> ! { let _ = syscall1(SYS_EXIT, code as u64); loop { core::hint::spin_loop(); } }

struct AegisBump;
static NEXT: AtomicUsize = AtomicUsize::new(0);
#[repr(align(16))] struct Heap([u8; 128 * 1024]);
static mut HEAP: Heap = Heap([0; 128 * 1024]);
unsafe impl GlobalAlloc for AegisBump {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        let base = core::ptr::addr_of_mut!(HEAP.0) as *mut u8 as usize;
        let mut cur = NEXT.load(Ordering::Relaxed);
        loop {
            let aligned = (cur + layout.align() - 1) & !(layout.align() - 1);
            let end = match aligned.checked_add(layout.size()) { Some(v) => v, None => return core::ptr::null_mut() };
            if end > 128 * 1024 { return core::ptr::null_mut(); }
            match NEXT.compare_exchange(cur, end, Ordering::SeqCst, Ordering::Relaxed) {
                Ok(_) => return (base + aligned) as *mut u8,
                Err(v) => cur = v,
            }
        }
    }
    unsafe fn dealloc(&self, _ptr: *mut u8, _layout: Layout) {}
}
#[global_allocator] static ALLOC: AegisBump = AegisBump;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    log(b"[aegis-rt] userspace panic\n");
    exit(127)
}
