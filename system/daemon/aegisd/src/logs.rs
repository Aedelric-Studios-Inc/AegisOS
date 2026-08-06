//! Log access for aegisd.

use std::fs;

const KERNEL_LOG: &str = "/var/log/aegis/kernel.log";
const SERVICE_LOG: &str = "/var/log/aegis/services.log";
const MAX_LINES: usize = 200;

pub fn get_logs() -> String {
    let kernel_lines = read_tail(KERNEL_LOG, MAX_LINES / 2);
    let service_lines = read_tail(SERVICE_LOG, MAX_LINES / 2);

    let all: Vec<String> = kernel_lines
        .into_iter()
        .chain(service_lines)
        .map(|line| format!("\"{}\"", line.replace('\\', "\\\\").replace('"', "\\\"")))
        .collect();

    format!(r#"{{"lines":[{}]}}"#, all.join(","))
}

fn read_tail(path: &str, max: usize) -> Vec<String> {
    match fs::read_to_string(path) {
        Ok(content) => {
            let lines: Vec<String> = content.lines().rev().take(max).map(str::to_owned).collect();
            lines.into_iter().rev().collect()
        }
        Err(_) => Vec::new(),
    }
}
