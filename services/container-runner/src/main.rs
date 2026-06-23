//! AegisOS container runner — lightweight OCI-compatible container runtime.

use std::process::Command;
use std::fs;
use std::path::Path;

const IMAGES_DIR: &str = "/var/lib/containers/images";
const ROOTFS_DIR: &str = "/var/lib/containers/rootfs";

fn main() {
    println!("[container-runner] service started");
    let args: Vec<String> = std::env::args().collect();

    if args.len() < 2 {
        run_daemon();
    } else {
        match args[1].as_str() {
            "run"  => cmd_run(&args[2..]),
            "list" => cmd_list(),
            cmd    => eprintln!("[container-runner] unknown command: {}", cmd),
        }
    }
}

/// Daemon mode: watch for container run requests in a queue directory.
fn run_daemon() {
    let queue = "/run/container-runner/queue";
    fs::create_dir_all(queue).ok();
    println!("[container-runner] watching queue at {}", queue);
    loop {
        if let Ok(entries) = fs::read_dir(queue) {
            for entry in entries.flatten() {
                if let Ok(content) = fs::read_to_string(entry.path()) {
                    let parts: Vec<&str> = content.trim().splitn(2, ' ').collect();
                    if !parts.is_empty() {
                        cmd_run(
                            &parts.iter().map(|s| s.to_string()).collect::<Vec<_>>()
                        );
                        fs::remove_file(entry.path()).ok();
                    }
                }
            }
        }
        std::thread::sleep(std::time::Duration::from_secs(1));
    }
}

/// Run a container from a pre-extracted rootfs.
fn cmd_run(args: &[String]) {
    if args.is_empty() {
        eprintln!("[container-runner] usage: run <image> [cmd...]");
        return;
    }
    let image = &args[0];
    let rootfs = Path::new(ROOTFS_DIR).join(image);

    if !rootfs.exists() {
        eprintln!("[container-runner] image rootfs not found: {}", rootfs.display());
        return;
    }

    // Use unshare to create a new PID/mount namespace (Linux-specific).
    let mut cmd_args = vec![
        "--fork".to_owned(),
        "--pid".to_owned(),
        "--mount-proc".to_owned(),
        "chroot".to_owned(),
        rootfs.to_str().unwrap_or("/").to_owned(),
    ];

    let inner_cmd = if args.len() > 1 { args[1..].to_vec() } else { vec!["/bin/sh".to_owned()] };
    cmd_args.extend(inner_cmd);

    println!("[container-runner] starting container from image '{}'", image);
    match Command::new("unshare").args(&cmd_args).status() {
        Ok(s) => println!("[container-runner] container exited: {}", s),
        Err(e) => eprintln!("[container-runner] exec failed: {}", e),
    }
}

fn cmd_list() {
    let dir = Path::new(IMAGES_DIR);
    match fs::read_dir(dir) {
        Ok(entries) => {
            println!("[container-runner] available images:");
            for e in entries.flatten() {
                println!("  {}", e.file_name().to_string_lossy());
            }
        }
        Err(e) => eprintln!("[container-runner] list: {}", e),
    }
}
