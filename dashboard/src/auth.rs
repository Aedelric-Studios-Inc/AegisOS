//! Session authentication middleware.

pub struct Session {
    pub token: String,
    pub user:  String,
}

pub fn authenticate(token: &str) -> Option<Session> {
    // TODO: validate HMAC-signed token
    let _ = token;
    None
}
