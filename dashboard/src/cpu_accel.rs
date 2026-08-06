//! CPU-oriented helpers for network feature scoring.

#[cfg(target_arch = "aarch64")]
use core::arch::asm;

pub fn dns_filter_score(blocked_queries: u32, total_queries: u32) -> u32 {
    ratio_score(blocked_queries, total_queries)
}

pub fn vpn_score(active_peers: u32, recent_handshakes: u32) -> u32 {
    ratio_score(recent_handshakes.min(active_peers), active_peers.max(1))
}

pub fn static_ip_score(reserved_assignments: u32, collisions: u32) -> u32 {
    let baseline = if reserved_assignments == 0 { 0 } else { 100 };
    saturating_sub(baseline, collisions.saturating_mul(35))
}

pub fn ad_block_score(blocked_ads: u32, ad_requests: u32) -> u32 {
    ratio_score(blocked_ads, ad_requests)
}

#[cfg(target_arch = "aarch64")]
fn ratio_score(numerator: u32, denominator: u32) -> u32 {
    if denominator == 0 {
        return 0;
    }

    let product = (numerator.min(denominator) as u64) * 100;
    let divisor = denominator as u64;
    let mut result: u64;
    unsafe {
        asm!(
            "udiv {result}, {product}, {divisor}",
            result = lateout(reg) result,
            product = in(reg) product,
            divisor = in(reg) divisor,
            options(pure, nomem, nostack)
        );
    }
    result.min(100) as u32
}

#[cfg(not(target_arch = "aarch64"))]
fn ratio_score(numerator: u32, denominator: u32) -> u32 {
    if denominator == 0 {
        0
    } else {
        numerator.min(denominator).saturating_mul(100) / denominator
    }
}

#[cfg(target_arch = "aarch64")]
fn saturating_sub(left: u32, right: u32) -> u32 {
    let mut result: u32;
    unsafe {
        asm!(
            "subs {result:w}, {left:w}, {right:w}",
            "csel {result:w}, {result:w}, wzr, hs",
            result = lateout(reg) result,
            left = in(reg) left,
            right = in(reg) right,
            options(pure, nomem, nostack)
        );
    }
    result
}

#[cfg(not(target_arch = "aarch64"))]
fn saturating_sub(left: u32, right: u32) -> u32 {
    left.saturating_sub(right)
}

#[cfg(test)]
mod tests {
    use super::{ad_block_score, dns_filter_score, static_ip_score, vpn_score};

    #[test]
    fn ratio_scores_cap_at_one_hundred() {
        assert_eq!(dns_filter_score(50, 50), 100);
        assert_eq!(ad_block_score(120, 100), 100);
    }

    #[test]
    fn vpn_score_tracks_recent_handshakes() {
        assert_eq!(vpn_score(4, 4), 100);
        assert_eq!(vpn_score(4, 2), 50);
    }

    #[test]
    fn static_ip_penalizes_collisions() {
        assert_eq!(static_ip_score(3, 0), 100);
        assert_eq!(static_ip_score(3, 1), 65);
        assert_eq!(static_ip_score(0, 1), 0);
    }
}
