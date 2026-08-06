#!/usr/bin/env bash
# Prove that QEMU's ARM64 -kernel path executes a given AegisOS Image/ELF.
# This is a fallback for hosts where UART/semihost stdout banners are swallowed.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
IMAGE="${1:-build/aegisos.bin}"
if [[ ! -f "$IMAGE" ]]; then
  echo "error: image not found: $IMAGE" >&2
  exit 2
fi
mkdir -p build/qemu-proof
TAG="$(basename "$IMAGE" | tr -c 'A-Za-z0-9_.-' '_')"
HMP="build/qemu-proof/${TAG}.hmp.sock"
RUN="build/qemu-proof/${TAG}.run.log"
MON="build/qemu-proof/${TAG}.monitor.log"
TRACE="build/qemu-proof/${TAG}.trace.log"
PY="build/qemu-proof/hmp_probe_kernel.py"
rm -f "$HMP"
: > "$RUN"; : > "$MON"; : > "$TRACE"
cat > "$PY" <<'PYINNER'
import os, socket, sys, time
sock_path, out_path = sys.argv[1], sys.argv[2]
cmds = [
    'info registers',
    'xp /12i 0x40000000',
    'xp /16i 0x40080000',
    'c',
    'stop',
    'info registers',
    'quit',
]
deadline = time.time() + 5
while not os.path.exists(sock_path):
    if time.time() > deadline:
        print(f'timeout waiting for monitor socket: {sock_path}', file=sys.stderr)
        sys.exit(2)
    time.sleep(0.05)
transcript = ''
with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
    s.connect(sock_path)
    s.settimeout(0.5)
    def recv_some(seconds=0.35):
        out=[]; end=time.time()+seconds
        while time.time() < end:
            try:
                data=s.recv(65536)
                if not data:
                    break
                out.append(data.decode('utf-8','replace'))
            except Exception:
                break
        return ''.join(out)
    transcript += recv_some()
    for cmd in cmds:
        try:
            s.sendall((cmd+'\n').encode())
        except BrokenPipeError:
            transcript += f'\n>>> {cmd}\n<monitor closed>\n'
            break
        time.sleep(0.25)
        transcript += f'\n>>> {cmd}\n' + recv_some()
open(out_path,'w',encoding='utf-8').write(transcript)
print(transcript)
PYINNER

qemu-system-aarch64 \
  -machine "${AEGISOS_QEMU_MACHINE:-virt,secure=off,virtualization=off,gic-version=2}" \
  -cpu "${AEGISOS_QEMU_CPU:-cortex-a57}" \
  -accel "${AEGISOS_QEMU_ACCEL:-tcg,thread=single}" \
  -smp 1 -m 1G -nographic \
  -semihosting-config enable=on,target=native \
  -no-reboot -no-shutdown \
  -monitor unix:"$HMP",server=on,wait=off \
  -serial none \
  -S \
  -d in_asm,exec,cpu,int,guest_errors,unimp \
  -D "$TRACE" \
  -kernel "$IMAGE" -append console=ttyAMA0,115200 \
  >"$RUN" 2>&1 &
QPID=$!
sleep 0.4
if ! kill -0 "$QPID" 2>/dev/null; then
  echo "qemu exited early; run log: $RUN" >&2
  cat "$RUN" >&2 || true
  exit 1
fi
python3 "$PY" "$HMP" "$MON" >/dev/null 2>&1 || true
sleep 0.2
kill "$QPID" 2>/dev/null || true
wait "$QPID" 2>/dev/null || true

if grep -Eq 'PC=0*40080000|PC=0*0000000040080000|0x40080000:|0x40080040:|/0000000040080000/|/0000000040080040/' "$TRACE" "$MON" 2>/dev/null; then
  echo "qemu proof: CPU reached AegisOS ARM64 entry at 0x40080000."
  echo "trace log: $TRACE"
  echo "monitor log: $MON"
  exit 0
fi
if grep -Eq '0x40000000:.*ldr|br[[:space:]]+x4|PC=0*40000000' "$TRACE" "$MON" 2>/dev/null; then
  echo "qemu proof: QEMU ARM64 -kernel stub executed, but AegisOS entry was not proven." >&2
  echo "trace log: $TRACE" >&2
  echo "monitor log: $MON" >&2
  exit 4
fi

echo "qemu proof failed: no entry trace found." >&2
echo "run log: $RUN" >&2
echo "monitor log: $MON" >&2
echo "trace log: $TRACE" >&2
exit 1
