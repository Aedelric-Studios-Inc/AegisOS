//! Security response headers for RustMyAdmin.

pub fn security_headers() -> &'static str {
    "X-Frame-Options: DENY\r\n\
     X-Content-Type-Options: nosniff\r\n\
     Referrer-Policy: no-referrer\r\n\
     Permissions-Policy: camera=(), microphone=(), geolocation=()\r\n\
     Content-Security-Policy: default-src 'self'; style-src 'self' 'unsafe-inline'; form-action 'self'; frame-ancestors 'none'; base-uri 'none'\r\n\
     Cache-Control: no-store"
}
