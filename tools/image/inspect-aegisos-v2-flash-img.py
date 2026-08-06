#!/usr/bin/env python3
from __future__ import annotations
import argparse, json, struct
from pathlib import Path
SECTOR=512

def parse_entry(data, idx):
    off=446+idx*16
    status, ptype, start_lba, sectors = struct.unpack('<BxxxBxxxII', data[off:off+16])
    return {'index':idx+1,'status':status,'type':ptype,'start_lba':start_lba,'sectors':sectors,'offset':start_lba*SECTOR,'size_bytes':sectors*SECTOR}

def main():
    ap=argparse.ArgumentParser(description='Inspect AegisOS v40 flash image layout')
    ap.add_argument('image')
    args=ap.parse_args()
    p=Path(args.image)
    with p.open('rb') as f:
        mbr=f.read(SECTOR)
    if mbr[510:512] != b'\x55\xaa':
        raise SystemExit('error: missing MBR signature 0x55aa')
    parts=[parse_entry(mbr,i) for i in range(4)]
    parts=[x for x in parts if x['type'] != 0 and x['sectors'] != 0]
    names=['AEGIS_BOOT','AEGIS_ROOT','AEGIS_CONFIG']
    if len(parts) < 3:
        raise SystemExit('error: expected at least 3 partitions')
    with p.open('rb') as f:
        for part,name in zip(parts,names):
            f.seek(part['offset'])
            head=f.read(64)
            if f'AEGISOS_PARTITION:{name}'.encode() not in head:
                raise SystemExit(f'error: missing {name} header at offset {part["offset"]}')
    print(json.dumps({'image':str(p),'mbr':'ok','partitions':parts[:3],'v40_layout':'ok'}, indent=2))
    print(f'AegisOS v2.0/v40 flash image inspection accepted: {p}')
if __name__ == '__main__': main()
