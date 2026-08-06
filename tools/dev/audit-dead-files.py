#!/usr/bin/env python3
"""Report C/ASM source files that are not part of the active kernel build."""
from pathlib import Path

root = Path('.')
compiled = set(map(str,
    list(root.glob('kernel/core/*.c')) +
    list(root.glob('kernel/memory/*.c')) +
    list(root.glob('kernel/ipc/*.c')) +
    list(root.glob('hal/arm64/*.c')) +
    list(root.glob('hal/drivers/uart/*.c')) +
    list(root.glob('hal/drivers/gpio/*.c')) +
    list(root.glob('hal/drivers/ethernet/*.c')) +
    list(root.glob('hal/drivers/storage/*.c')) +
    list(root.glob('hal/drivers/usb/*.c')) +
    list(root.glob('hal/drivers/watchdog/*.c')) +
    [root / 'hal/boards/bastion/board.c'] +
    list(root.glob('net/*.c')) +
    list(root.glob('fs/*.c')) +
    list(root.glob('boot/arm64/*.S'))
))

print('C/S files not compiled by the active kernel Makefile:')
for p in sorted(root.rglob('*')):
    if p.suffix in ('.c', '.S') and 'tests' not in p.parts and str(p) not in compiled:
        print(f'  {p}')
