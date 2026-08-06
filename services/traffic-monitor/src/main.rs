//! AegisOS traffic monitor — reports live interface counters from `/proc/net/dev`.

use std::collections::HashMap;
use std::fs;
use std::thread;
use std::time::Duration;

const REPORT_INTERVAL: Duration = Duration::from_secs(5);

type Counters = (u64, u64);

fn main() {
    println!("[traffic-monitor] service started");
    let mut previous = read_counters();

    loop {
        thread::sleep(REPORT_INTERVAL);
        let current = read_counters();
        let mut total_rx_delta = 0_u64;
        let mut total_tx_delta = 0_u64;

        for (interface, (rx, tx)) in &current {
            let (old_rx, old_tx) = previous.get(interface).copied().unwrap_or((0, 0));
            let rx_delta = rx.saturating_sub(old_rx);
            let tx_delta = tx.saturating_sub(old_tx);
            total_rx_delta = total_rx_delta.saturating_add(rx_delta);
            total_tx_delta = total_tx_delta.saturating_add(tx_delta);
            if rx_delta > 0 || tx_delta > 0 {
                println!(
                    "[traffic-monitor] interface={} rx_bytes={} tx_bytes={} interval_rx={} interval_tx={}",
                    interface, rx, tx, rx_delta, tx_delta
                );
            }
        }

        println!(
            "[traffic-monitor] interval totals rx_bytes={} tx_bytes={}",
            total_rx_delta, total_tx_delta
        );
        previous = current;
    }
}

fn read_counters() -> HashMap<String, Counters> {
    let mut counters = HashMap::new();
    let Ok(content) = fs::read_to_string("/proc/net/dev") else {
        eprintln!("[traffic-monitor] cannot read /proc/net/dev");
        return counters;
    };

    for line in content.lines().skip(2) {
        let Some((interface, values)) = line.split_once(':') else {
            continue;
        };
        let fields = values.split_whitespace().collect::<Vec<_>>();
        if fields.len() < 16 {
            continue;
        }
        let Ok(rx_bytes) = fields[0].parse::<u64>() else {
            continue;
        };
        let Ok(tx_bytes) = fields[8].parse::<u64>() else {
            continue;
        };
        counters.insert(interface.trim().to_owned(), (rx_bytes, tx_bytes));
    }
    counters
}
