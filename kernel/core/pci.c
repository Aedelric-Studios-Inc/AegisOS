/* SPDX-License-Identifier: Proprietary */
#include "pci.h"

#define PCI_MAX_DEVICES 64U
static u64 g_ecam;
static u8 g_bus_start,g_bus_end;
static aegis_pci_device_t g_devices[PCI_MAX_DEVICES];
static u32 g_count;

static void zero(void*p,u64 n){u8*b=p;for(u64 i=0;i<n;i++)b[i]=0;}
static volatile u32 *cfg(u8 b,u8 d,u8 f,u16 o){return (volatile u32*)(uptr)(g_ecam+((u64)b<<20)+((u64)d<<15)+((u64)f<<12)+(o&~3U));}
static u32 read32(u8 b,u8 d,u8 f,u16 o){return *cfg(b,d,f,o);}
static void write32(u8 b,u8 d,u8 f,u16 o,u32 v){*cfg(b,d,f,o)=v;}

void pci_init(u64 base,u8 first,u8 last){
    g_ecam=base;g_bus_start=first;g_bus_end=last<first?first:last;g_count=0;zero(g_devices,sizeof(g_devices));
}

static void read_bars(aegis_pci_device_t*x){
    for(u32 i=0;i<6U;i++){
        u32 slot=i;
        u32 lo=read32(x->bus,x->device,x->function,(u16)(0x10U+slot*4U));
        if(lo==0U||lo==0xffffffffU){x->bar[slot]=0U;continue;}
        if(lo&1U){x->bar[slot]=(u64)(lo&~3U);continue;}
        u64 value=(u64)(lo&~0xfU);
        u32 kind=(lo>>1U)&3U;
        if(kind==2U&&slot+1U<6U){
            u32 hi=read32(x->bus,x->device,x->function,(u16)(0x10U+(slot+1U)*4U));
            value|=((u64)hi<<32U);
            x->bar[slot]=value;
            x->bar[slot+1U]=0U;
            i++;
        }else{
            x->bar[slot]=value;
        }
    }
}

int pci_scan(void){
    if(!g_ecam)return AEGIS_EINVAL;
    g_count=0;
    for(u32 b=g_bus_start;b<=g_bus_end&&g_count<PCI_MAX_DEVICES;b++){
        for(u32 d=0;d<32U&&g_count<PCI_MAX_DEVICES;d++){
            u32 header0=0;
            for(u32 f=0;f<8U&&g_count<PCI_MAX_DEVICES;f++){
                u32 id=read32((u8)b,(u8)d,(u8)f,0U);
                if((id&0xffffU)==0xffffU){if(f==0U)break;continue;}
                u32 cls=read32((u8)b,(u8)d,(u8)f,8U);
                aegis_pci_device_t*x=&g_devices[g_count++];zero(x,sizeof(*x));
                x->bus=(u8)b;x->device=(u8)d;x->function=(u8)f;
                x->vendor_id=(u16)(id&0xffffU);x->device_id=(u16)(id>>16U);
                x->class_code=(u8)(cls>>24U);x->subclass=(u8)(cls>>16U);x->prog_if=(u8)(cls>>8U);
                read_bars(x);
                x->irq=read32(x->bus,x->device,x->function,0x3cU)&0xffU;
                if(f==0U){header0=read32(x->bus,x->device,x->function,0x0cU);if(((header0>>16U)&0x80U)==0U)break;}
            }
        }
        if(b==255U)break;
    }
    return g_count?AEGIS_OK:AEGIS_ENOENT;
}

u32 pci_device_count(void){return g_count;}
const aegis_pci_device_t*pci_device(u32 i){return i<g_count?&g_devices[i]:NULL;}
const aegis_pci_device_t*pci_find_class(u8 c,u8 s,u8 p,u32 o){for(u32 i=0;i<g_count;i++)if(g_devices[i].class_code==c&&g_devices[i].subclass==s&&g_devices[i].prog_if==p){if(!o)return&g_devices[i];o--;}return NULL;}
int pci_enable_memory_busmaster(const aegis_pci_device_t*d){if(!d)return AEGIS_EINVAL;u32 v=read32(d->bus,d->device,d->function,4U);v|=0x6U;write32(d->bus,d->device,d->function,4U,v);return AEGIS_OK;}
