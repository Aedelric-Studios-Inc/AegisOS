//! Backup management for aegisd.

use std::fs;

const BACKUP_DIR: &str = "/var/backups/aegis";

pub fn list_backups() -> String {
    let entries = match fs::read_dir(BACKUP_DIR) {
        Ok(rd) => rd
            .filter_map(|e| e.ok())
            .filter_map(|e| {
                let meta = e.metadata().ok()?;
                let name = e.file_name().into_string().ok()?;
                let size = meta.len();
                let modified = meta
                    .modified()
                    .ok()
                    .and_then(|t| t.duration_since(std::time::UNIX_EPOCH).ok())
                    .map(|d| d.as_secs())
                    .unwrap_or(0);
                Some(format!(
                    r#"{{"name":"{}","size_bytes":{},"modified_unix":{}}}"#,
                    name, size, modified
                ))
            })
            .collect::<Vec<_>>(),
        Err(_) => Vec::new(),
    };
    format!(r#"{{"backups":[{}]}}"#, entries.join(","))
}
