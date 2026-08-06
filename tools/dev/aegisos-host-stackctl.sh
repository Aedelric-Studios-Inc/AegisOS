#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$ROOT"

case ${1:-status} in
    start|stop|restart|status)
        sudo systemctl "$1" aegisos-dev-stack.service
        ;;
    logs)
        exec sudo journalctl -u aegisos-dev-stack.service -f
        ;;
    services)
        exec sudo ./system/cli/aegisctl/target/release/system-aegisctl service list
        ;;
    router)
        exec sudo ./system/cli/aegisctl/target/release/system-aegisctl router status
        ;;
    *)
        echo "Usage: $0 {start|stop|restart|status|logs|services|router}" >&2
        exit 2
        ;;
esac
