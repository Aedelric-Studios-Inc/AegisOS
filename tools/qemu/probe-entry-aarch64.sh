#!/usr/bin/env bash
# v17: Probe QEMU entry state with the HMP monitor instead of relying on a
# guest banner.  This answers: did QEMU set CPU0 PC to our payload?
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
AEGISOS_QEMU_TIMEOUT=1 tools/qemu/sanity-aarch64.sh >/dev/null 2>&1 || true
mkdir -p build/qemu-sanity

PY_PROBE="build/qemu-sanity/hmp_probe.py"
cat > "$PY_PROBE" <<'PY'
import os, socket, sys, time
sock_path = sys.argv[1]
out_path = sys.argv[2]
commands = sys.argv[3:]
deadline = time.time() + 5
while not os.path.exists(sock_path):
    if time.time() > deadline:
        print(f"timeout waiting for monitor socket: {sock_path}", file=sys.stderr)
        sys.exit(2)
    time.sleep(0.05)
with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
    s.connect(sock_path)
    s.settimeout(1.0)
    def recv_some():
        chunks=[]
        end=time.time()+0.4
        while time.time() < end:
            try:
                data=s.recv(65536)
                if not data:
                    break
                chunks.append(data.decode('utf-8','replace'))
            except TimeoutError:
                break
            except socket.timeout:
                break
        return ''.join(chunks)
    transcript = recv_some()
    for cmd in commands:
        try:
            s.sendall((cmd + "\n").encode())
        except BrokenPipeError:
            transcript += f"\n>>> {cmd}\n<monitor closed: guest/QEMU exited>\n"
            break
        time.sleep(0.25)
        transcript += f"\n>>> {cmd}\n" + recv_some()
with open(out_path, 'w', encoding='utf-8') as f:
    f.write(transcript)
print(transcript)
PY

run_probe() {
  local tag="$1"; shift
  local hmp="build/qemu-sanity/probe-${tag}.hmp.sock"
  local log="build/qemu-sanity/probe-${tag}.run.log"
  local mon="build/qemu-sanity/probe-${tag}.monitor.log"
  local trace="build/qemu-sanity/probe-${tag}.trace.log"
  rm -f "$hmp"
  : > "$log"; : > "$mon"; : > "$trace"
  echo "== HMP entry probe: $tag =="
  set +e
  qemu-system-aarch64 \
    -machine "${AEGISOS_QEMU_MACHINE:-virt,secure=off,virtualization=off,gic-version=2}" \
    -cpu "${AEGISOS_QEMU_CPU:-cortex-a57}" \
    -accel "${AEGISOS_QEMU_ACCEL:-tcg,thread=single}" \
    -smp 1 \
    -m 1G \
    -nographic \
    -semihosting-config enable=on,target=native \
    -no-reboot -no-shutdown \
    -monitor unix:"$hmp",server=on,wait=off \
    -serial none \
    -S \
    -d in_asm,exec,cpu,int,guest_errors,unimp \
    -D "$trace" \
    "$@" \
    >"$log" 2>&1 &
  local qpid=$!
  set -e
  sleep 0.4
  if ! kill -0 "$qpid" 2>/dev/null; then
    echo "qemu exited early; run log:"
    cat "$log" || true
    return 1
  fi
  python3 "$PY_PROBE" "$hmp" "$mon" \
    'info registers' \
    'xp /8i 0x40080000' \
    'c' \
    'info registers' \
    'stop' \
    'info registers' \
    'quit' || true
  sleep 0.2
  kill "$qpid" 2>/dev/null || true
  wait "$qpid" 2>/dev/null || true
  echo "run log: $log"
  echo "monitor log: $mon"
  echo "trace log: $trace"
  echo "monitor PC highlights:"
  grep -Ei 'pc|PSTATE|x0|x1|0x40080000|AArch' "$mon" | head -40 || true
  echo "first trace lines:"
  sed -n '1,80p' "$trace" || true
}

# Probe the proven paths first.
run_probe kernel-image-bin \
  -kernel build/qemu-sanity/sanity_image.bin -append console=ttyAMA0,115200 || true
run_probe bios-bare \
  -bios build/qemu-sanity/sanity_semihost_exit_bios.bin || true

# Loader default path can overlap QEMU's generated DTB on this host, so keep it
# as an optional diagnostic rather than part of the normal pass/fail path.
if [[ "${AEGISOS_QEMU_PROBE_LOADERS:-0}" == "1" ]]; then
  run_probe loader-raw-no-bios \
    -device loader,file=build/qemu-sanity/sanity_semihost_exit_ram.bin,addr=0x40080000,cpu-num=0,force-raw=on || true
  run_probe loader-elf-no-bios \
    -device loader,file=build/qemu-sanity/sanity_semihost_exit_ram.elf,cpu-num=0 || true
else
  echo "note: skipped generic-loader probes; set AEGISOS_QEMU_PROBE_LOADERS=1 to confirm the DTB overlap behaviour."
fi

echo "Expected v18 result: -kernel image trace should show QEMU stub at 0x40000000 and payload at 0x40080000."
echo "BIOS semihost trace should show semihost call 0x4 and SYS_EXIT 0x18."
