#![no_std]
#![no_main]
use aegis_rt::*;
const RESPONSE:&[u8]=b"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: 60\r\n\r\n{\"service\":\"aegis-dashboard\",\"status\":\"native-aegisos\"}\n";
#[no_mangle]pub extern "C" fn _start()->!{
 log(b"[dashboard:rust] native dashboard starting\n");
 if service_ready(b"dashboard\0")<0{exit(1)}
 let s=socket(SOCK_STREAM);if s<0||bind(s,8080)<0||listen(s)<0{exit(2)}
 log(b"[dashboard:rust] listening on 0.0.0.0:8080\n");
 loop{let c=accept(s);if c<0{yield_now();continue}let mut req=[0u8;1024];let _=recv(c,&mut req);let _=send(c,RESPONSE);let _=close(c);}
}
