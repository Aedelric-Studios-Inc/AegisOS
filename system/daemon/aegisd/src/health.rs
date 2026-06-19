//! Health reporting for aegisd.

use std::time::Instant;

pub fn get_health(start: Instant) -> String {
    let uptime_secs = start.elapsed().as_secs();
    format!(
        r#"{{"status":"ok","uptime_seconds":{}}}"#,
        uptime_secs
    )
}
