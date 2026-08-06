#!/usr/bin/env python3
"""Build a dependency-free GPT/FAT32 AegisOS ARM64 UEFI disk image.

The ESP contains:
  /EFI/BOOT/BOOTAA64.EFI
  /EFI/AEGIS/AEGISOS.ELF
Additional GPT partitions reserve persistent root/config space for AegisFS.
"""
from __future__ import annotations
import argparse, binascii, hashlib, math, os, struct, uuid
from pathlib import Path

SECTOR=512
ESP_GUID=uuid.UUID('c12a7328-f81f-11d2-ba4b-00a0c93ec93b')
LINUX_FS_GUID=uuid.UUID('0fc63daf-8483-4772-8e79-3d69d8477de4')
AEGIS_CFG_GUID=uuid.UUID('9d275380-40ad-11ee-be56-0242ac120002')

def guid_le(g: uuid.UUID)->bytes: return g.bytes_le

def short_entry(name:str, attr:int, cluster:int, size:int=0)->bytes:
    if name in ('.','..'):
        base=(name.ljust(11)).encode('ascii')
    else:
        stem, dot, ext=name.partition('.')
        base=(stem.upper().ljust(8)[:8]+ext.upper().ljust(3)[:3]).encode('ascii')
    e=bytearray(32); e[:11]=base; e[11]=attr
    struct.pack_into('<H',e,20,(cluster>>16)&0xffff); struct.pack_into('<H',e,26,cluster&0xffff)
    struct.pack_into('<I',e,28,size); return bytes(e)

class FAT32:
    def __init__(self,sectors:int):
        self.total=sectors; self.reserved=32; self.fats=2; self.spc=1
        # converge FAT size
        fat=math.ceil((sectors + 2) * 4 / SECTOR)
        while True:
            data=sectors-self.reserved-self.fats*fat
            clusters=data//self.spc
            need=math.ceil((clusters+2)*4/SECTOR)
            if need <= fat: break
            fat=need
        self.fat_sectors=fat; self.data_start=self.reserved+self.fats*fat
        self.clusters=(sectors-self.data_start)//self.spc
        self.image=bytearray(sectors*SECTOR)
        self.fat=[0]*(self.clusters+2); self.fat[0]=0x0ffffff8; self.fat[1]=0x0fffffff
        self.next_cluster=2
    def alloc(self,count:int)->list[int]:
        if count<1: count=1
        out=list(range(self.next_cluster,self.next_cluster+count)); self.next_cluster+=count
        if self.next_cluster>=len(self.fat): raise RuntimeError('ESP full')
        for a,b in zip(out,out[1:]): self.fat[a]=b
        self.fat[out[-1]]=0x0fffffff
        return out
    def cluster_off(self,c:int)->int: return (self.data_start+(c-2)*self.spc)*SECTOR
    def write_chain(self,clusters:list[int],data:bytes):
        pos=0
        for c in clusters:
            off=self.cluster_off(c); chunk=data[pos:pos+SECTOR*self.spc]
            self.image[off:off+len(chunk)]=chunk; pos+=len(chunk)
    def add_file(self,data:bytes)->tuple[int,int]:
        chain=self.alloc(math.ceil(len(data)/(SECTOR*self.spc)) or 1); self.write_chain(chain,data); return chain[0],len(data)
    def write_dir(self,cluster:int,entries:list[bytes]): self.write_chain([cluster],b''.join(entries)+b'\0'*32)
    def finish(self,volume_id:int=0xAE610501, hidden_sectors:int=0)->bytes:
        b=self.image
        # BPB/boot sector
        b[0:3]=b'\xeb\x58\x90'; b[3:11]=b'AEGISOS '
        struct.pack_into('<H',b,11,SECTOR); b[13]=self.spc; struct.pack_into('<H',b,14,self.reserved)
        b[16]=self.fats; struct.pack_into('<H',b,17,0); struct.pack_into('<H',b,19,0); b[21]=0xf8
        struct.pack_into('<H',b,22,0); struct.pack_into('<H',b,24,63); struct.pack_into('<H',b,26,255)
        struct.pack_into('<I',b,28,hidden_sectors); struct.pack_into('<I',b,32,self.total); struct.pack_into('<I',b,36,self.fat_sectors)
        struct.pack_into('<H',b,40,0); struct.pack_into('<H',b,42,0); struct.pack_into('<I',b,44,2)
        struct.pack_into('<H',b,48,1); struct.pack_into('<H',b,50,6); b[64]=0x80; b[66]=0x29
        struct.pack_into('<I',b,67,volume_id); b[71:82]=b'AEGISOS ESP'; b[82:90]=b'FAT32   '
        b[510:512]=b'\x55\xaa'
        # FSInfo
        fs=SECTOR; struct.pack_into('<I',b,fs+0,0x41615252); struct.pack_into('<I',b,fs+484,0x61417272)
        struct.pack_into('<I',b,fs+488,len(self.fat)-self.next_cluster); struct.pack_into('<I',b,fs+492,self.next_cluster)
        struct.pack_into('<I',b,fs+508,0xaa550000)
        b[6*SECTOR:7*SECTOR]=b[:SECTOR]
        b[7*SECTOR:8*SECTOR]=b[SECTOR:2*SECTOR]
        # FATs
        raw=bytearray(self.fat_sectors*SECTOR)
        for i,v in enumerate(self.fat): struct.pack_into('<I',raw,i*4,v)
        for n in range(self.fats):
            off=(self.reserved+n*self.fat_sectors)*SECTOR; b[off:off+len(raw)]=raw
        return bytes(b)

def gpt_entry(type_guid:uuid.UUID, unique:uuid.UUID, first:int,last:int,name:str)->bytes:
    name16=name.encode('utf-16le')[:72].ljust(72,b'\0')
    return guid_le(type_guid)+guid_le(unique)+struct.pack('<QQQ',first,last,0)+name16

def build(out:Path, efi:Path, kernel:Path, size_mib:int):
    total=size_mib*1024*1024//SECTOR
    if total<262144: raise ValueError('disk must be at least 128 MiB')
    first_usable=2048; backup_entries_lba=total-33; last_usable=backup_entries_lba-1
    esp_first=2048; esp_sectors=131072; esp_last=esp_first+esp_sectors-1
    root_first=esp_last+1; root_sectors=max(65536,(last_usable-root_first+1)*2//3); root_last=min(last_usable,root_first+root_sectors-1)
    cfg_first=root_last+1; cfg_last=last_usable
    if cfg_first>=cfg_last: raise ValueError('disk too small for config partition')

    fat=FAT32(esp_sectors)
    root=fat.alloc(1)[0] # cluster 2
    efi_dir=fat.alloc(1)[0]; boot_dir=fat.alloc(1)[0]; aegis_dir=fat.alloc(1)[0]
    boot_c,boot_size=fat.add_file(efi.read_bytes()); kern_c,kern_size=fat.add_file(kernel.read_bytes())
    label=short_entry('AEGISOS',0x08,0,0)
    fat.write_dir(root,[label,short_entry('EFI',0x10,efi_dir)])
    fat.write_dir(efi_dir,[short_entry('.',0x10,efi_dir),short_entry('..',0x10,root),short_entry('BOOT',0x10,boot_dir),short_entry('AEGIS',0x10,aegis_dir)])
    fat.write_dir(boot_dir,[short_entry('.',0x10,boot_dir),short_entry('..',0x10,efi_dir),short_entry('BOOTAA64.EFI',0x20,boot_c,boot_size)])
    fat.write_dir(aegis_dir,[short_entry('.',0x10,aegis_dir),short_entry('..',0x10,efi_dir),short_entry('AEGISOS.ELF',0x20,kern_c,kern_size)])
    esp=fat.finish(hidden_sectors=esp_first)

    disk=bytearray(total*SECTOR)
    # protective MBR
    disk[446:462]=b'\0\0\2\0\xee\xff\xff\xff'+struct.pack('<II',1,min(total-1,0xffffffff)); disk[510:512]=b'\x55\xaa'
    seed=hashlib.sha256(efi.read_bytes()+kernel.read_bytes()+str(size_mib).encode()+os.environ.get('SOURCE_DATE_EPOCH','0').encode()).hexdigest()
    ns=uuid.UUID(seed[:32])
    disk_guid=uuid.uuid5(ns,'AegisOS-2.0.0-pre.1-disk'); entries=bytearray(128*128)
    parts=[
        (ESP_GUID,uuid.uuid5(ns,'AEGIS_BOOT'),esp_first,esp_last,'AEGIS_BOOT'),
        (LINUX_FS_GUID,uuid.uuid5(ns,'AEGIS_ROOT'),root_first,root_last,'AEGIS_ROOT'),
        (AEGIS_CFG_GUID,uuid.uuid5(ns,'AEGIS_CONFIG'),cfg_first,cfg_last,'AEGIS_CONFIG')]
    for i,p in enumerate(parts): entries[i*128:(i+1)*128]=gpt_entry(*p)
    entries_crc=binascii.crc32(entries)&0xffffffff
    def header(current:int,backup:int,entries_lba:int)->bytes:
        h=bytearray(SECTOR); h[:8]=b'EFI PART'; struct.pack_into('<I',h,8,0x00010000); struct.pack_into('<I',h,12,92)
        struct.pack_into('<QQQQ',h,24,current,backup,first_usable,last_usable); h[56:72]=guid_le(disk_guid)
        struct.pack_into('<QIII',h,72,entries_lba,128,128,entries_crc); struct.pack_into('<I',h,16,0)
        struct.pack_into('<I',h,16,binascii.crc32(h[:92])&0xffffffff); return bytes(h)
    disk[2*SECTOR:(2+32)*SECTOR]=entries
    disk[SECTOR:2*SECTOR]=header(1,total-1,2)
    disk[backup_entries_lba*SECTOR:(backup_entries_lba+32)*SECTOR]=entries
    disk[(total-1)*SECTOR:total*SECTOR]=header(total-1,1,backup_entries_lba)
    disk[esp_first*SECTOR:(esp_first+esp_sectors)*SECTOR]=esp
    out.parent.mkdir(parents=True,exist_ok=True); out.write_bytes(disk)
    manifest=out.with_suffix(out.suffix+'.manifest')
    manifest.write_text(f'disk_guid={disk_guid}\nsize_bytes={len(disk)}\nsha256={hashlib.sha256(disk).hexdigest()}\nesp_lba={esp_first}-{esp_last}\nroot_lba={root_first}-{root_last}\nconfig_lba={cfg_first}-{cfg_last}\n')

if __name__=='__main__':
    ap=argparse.ArgumentParser(); ap.add_argument('--efi',required=True,type=Path); ap.add_argument('--kernel',required=True,type=Path); ap.add_argument('--out',required=True,type=Path); ap.add_argument('--size-mib',type=int,default=256)
    a=ap.parse_args(); build(a.out,a.efi,a.kernel,a.size_mib)
