//! Security response headers for rustmyadmin.

/// Return a newline-separated list of security headers to add to every response.
pub fn security_headers() -> &'static str {
    "X-Frame-Options: DENY\r\n\
     X-Content-Type-Options: nosniff\r\n\
     X-XSS-Protection: 1; mode=block\r\n\
     Referrer-Policy: no-referrer\r\n\
     Content-Security-Policy: default-src 'self'\r\n\
     Cache-Control: no-store"
}
