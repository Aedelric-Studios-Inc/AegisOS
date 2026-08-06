//! AegisOS image builder — assembles a flashable disk image.

use std::fs::{self, File, OpenOptions};
use std::io::{self, Read, Write, Seek, SeekFrom};
use std::path::PathBuf;
use std::env;

/// Partition descriptor parsed from partitions.toml (minimal format).
#[derive(Debug)]
struct PartitionDef {
    name:   String,
    offset: u64,  // bytes from start of image
    size:   u64,  // bytes
    source: Option<PathBuf>,
}

fn main() {
    println!("[image-builder] Building AegisOS image...");
    let args: Vec<String> = env::args().collect();
    let config_path = args.get(1).map(String::as_str).unwrap_or("partitions.toml");
    let output_path = args.get(2).map(String::as_str).unwrap_or("aegisos.img");

    let partitions = match load_partitions(config_path) {
        Ok(p) => p,
        Err(e) => {
            eprintln!("[image-builder] config error: {}", e);
            // Use built-in default layout if config is missing
            default_partitions()
        }
    };

    if let Err(e) = build_image(&partitions, output_path) {
        eprintln!("[image-builder] build failed: {}", e);
        std::process::exit(1);
    }
    println!("[image-builder] image written to {}", output_path);
}

fn default_partitions() -> Vec<PartitionDef> {
    vec![
        PartitionDef { name: "boot".into(),   offset: 0,              size: 64 * 1024 * 1024, source: None },
        PartitionDef { name: "rootfs_a".into(), offset: 64*1024*1024, size: 512*1024*1024,    source: None },
        PartitionDef { name: "rootfs_b".into(), offset: 576*1024*1024,size: 512*1024*1024,    source: None },
        PartitionDef { name: "data".into(),   offset: 1088*1024*1024, size: 256*1024*1024,    source: None },
    ]
}

fn load_partitions(path: &str) -> Result<Vec<PartitionDef>, String> {
    let content = fs::read_to_string(path)
        .map_err(|e| format!("{}: {}", path, e))?;
    let mut parts = Vec::new();
    let mut cur: Option<PartitionDef> = None;

    for line in content.lines() {
        let line = line.trim();
        if line == "[[partition]]" {
            if let Some(p) = cur.take() { parts.push(p); }
            cur = Some(PartitionDef { name: String::new(), offset: 0, size: 0, source: None });
            continue;
        }
        if let Some(ref mut p) = cur {
            if let Some(v) = kv_str(line, "name")   { p.name   = v.to_owned(); }
            if let Some(v) = kv_u64(line, "offset") { p.offset = v; }
            if let Some(v) = kv_u64(line, "size")   { p.size   = v; }
            if let Some(v) = kv_str(line, "source") { p.source = Some(PathBuf::from(v)); }
        }
    }
    if let Some(p) = cur { parts.push(p); }
    Ok(parts)
}

fn kv_str<'a>(line: &'a str, key: &str) -> Option<&'a str> {
    let prefix = format!("{} =", key);
    if line.starts_with(&prefix) {
        Some(line[prefix.len()..].trim().trim_matches('"'))
    } else { None }
}

fn kv_u64(line: &str, key: &str) -> Option<u64> {
    kv_str(line, key).and_then(|v| v.parse().ok())
}

fn build_image(partitions: &[PartitionDef], output: &str) -> io::Result<()> {
    // Compute total image size
    let image_size = partitions.iter()
        .map(|p| p.offset + p.size)
        .max()
        .unwrap_or(0);

    println!("[image-builder] image size: {} MiB", image_size / (1024 * 1024));

    // Create sparse image file
    let mut img = OpenOptions::new()
        .write(true).create(true).truncate(true)
        .open(output)?;

    // Extend to full size (sparse on most filesystems)
    img.seek(SeekFrom::Start(image_size.saturating_sub(1)))?;
    img.write_all(&[0u8])?;
    img.seek(SeekFrom::Start(0))?;

    // Write each partition content
    for part in partitions {
        println!("[image-builder]   partition '{}' @ offset 0x{:x} ({} MiB)",
            part.name, part.offset, part.size / (1024 * 1024));
        if let Some(ref src) = part.source {
            if src.exists() {
                let mut src_file = File::open(src)?;
                img.seek(SeekFrom::Start(part.offset))?;
                let mut written = 0u64;
                let mut buf = [0u8; 65536];
                loop {
                    let n = src_file.read(&mut buf)?;
                    if n == 0 { break; }
                    if written + n as u64 > part.size {
                        eprintln!("[image-builder] WARNING: source exceeds partition size for '{}'", part.name);
                        break;
                    }
                    img.write_all(&buf[..n])?;
                    written += n as u64;
                }
                println!("[image-builder]     wrote {} bytes from {}", written, src.display());
            } else {
                eprintln!("[image-builder]   source not found: {}", src.display());
            }
        }
    }

    img.flush()?;
    Ok(())
}
