#![no_std]
#![no_main]
use aegis_rt::*;
const RESPONSE:&[u8]=b"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: 68\r\n\r\n{\"service\":\"RustMyAdmin\",\"runtime\":\"native-aegisos\",\"tls\":false}\n";
#[no_mangle]pub extern "C" fn _start()->!{
 log(b"[rustmyadmin:rust] native service starting\n");
 if service_ready(b"rustmyadmin\0")<0{exit(1)}
 let fd=open(b"/persist/rustmyadmin.state\0",2);if fd>=0{let _=write(fd as u64,b"booted\n");let _=close(fd);}
 let s=socket(SOCK_STREAM);if s<0||bind(s,8081)<0||listen(s)<0{exit(2)}
 log(b"[rustmyadmin:rust] listening on 0.0.0.0:8081 (HTTP development endpoint; PR1 TLS gate remains fail-closed)\n");
 loop{let c=accept(s);if c<0{yield_now();continue}let mut req=[0u8;2048];let _=recv(c,&mut req);let _=send(c,RESPONSE);let _=close(c);}
}
