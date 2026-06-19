//! Rate limiting for rustmyadmin.

use std::collections::HashMap;
use std::net::IpAddr;
use std::sync::Mutex;
use std::time::{Duration, Instant};

const MAX_REQUESTS: u32 = 60;
const WINDOW: Duration  = Duration::from_secs(60);

struct Bucket {
    count:      u32,
    window_end: Instant,
}

static BUCKETS: Mutex<Option<HashMap<IpAddr, Bucket>>> = Mutex::new(None);

/// Returns `true` if the request should be allowed, `false` if rate-limited.
pub fn allow(ip: IpAddr) -> bool {
    let mut guard = BUCKETS.lock().unwrap_or_else(|e| e.into_inner());
    let map = guard.get_or_insert_with(HashMap::new);

    let now = Instant::now();
    let bucket = map.entry(ip).or_insert_with(|| Bucket {
        count:      0,
        window_end: now + WINDOW,
    });

    if now >= bucket.window_end {
        bucket.count      = 0;
        bucket.window_end = now + WINDOW;
    }

    bucket.count += 1;
    bucket.count <= MAX_REQUESTS
}
