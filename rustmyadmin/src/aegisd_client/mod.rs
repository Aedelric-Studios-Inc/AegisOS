//! Client for the privileged aegisd Unix-socket API.

mod client;
pub mod errors;
pub mod models;

use std::time::Duration;

pub(super) const DEFAULT_SOCKET_PATH: &str = "/run/aegisd.sock";

pub(super) fn socket_path() -> String {
    std::env::var("AEGISD_SOCKET").unwrap_or_else(|_| DEFAULT_SOCKET_PATH.to_owned())
}
pub(super) const TIMEOUT: Duration = Duration::from_secs(5);

pub struct AegisdClient;
