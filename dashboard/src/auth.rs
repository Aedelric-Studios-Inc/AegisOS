//! Session authentication middleware.

use std::net::IpAddr;

pub struct Session {
    pub token: String,
    pub user: String,
}

pub fn authenticate(token: &str) -> Option<Session> {
    let expected = std::env::var("AEGIS_DASHBOARD_TOKEN").ok()?;
    let candidate = token.trim().trim_start_matches("Bearer ").trim();
    if candidate == expected {
        Some(Session {
            token: candidate.to_owned(),
            user: "admin".to_owned(),
        })
    } else {
        None
    }
}

pub fn authorize_api_request(token: Option<&str>, client_ip: IpAddr) -> bool {
    match std::env::var("AEGIS_DASHBOARD_TOKEN") {
        Ok(_) => token
            .and_then(authenticate)
            .is_some_and(|session| !session.user.is_empty() && !session.token.is_empty()),
        Err(_) => client_ip.is_loopback(),
    }
}
