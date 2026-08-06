/* SPDX-License-Identifier: Proprietary */
#include "rtc.h"
#include "device_tree.h"

#define PL031_DR 0x000U
#define PL031_CR 0x00cU
#define PL031_PERIPH_ID0 0xfe0U

static u64 g_rtc_base;
static bool g_ready;
static u32 mmio_read32(u64 base,u32 off){return *(volatile u32*)(uptr)(base+off);}
static void mmio_write32(u64 base,u32 off,u32 value){*(volatile u32*)(uptr)(base+off)=value;}

void rtc_init(void){
    g_ready=false;g_rtc_base=0U;
    const aegis_dtb_platform_t*p=device_tree_platform();
    if(!p||!p->valid||!p->rtc_base)return;
    g_rtc_base=p->rtc_base;
    if((mmio_read32(g_rtc_base,PL031_PERIPH_ID0)&0xffU)!=0x31U){g_rtc_base=0U;return;}
    mmio_write32(g_rtc_base,PL031_CR,1U);g_ready=true;
}
bool rtc_ready(void){return g_ready;}
u64 rtc_unix_seconds(void){return g_ready?(u64)mmio_read32(g_rtc_base,PL031_DR):0ULL;}
u64 monotonic_nanoseconds(void){u64 c,f;__asm__ volatile("mrs %0, cntpct_el0":"=r"(c));__asm__ volatile("mrs %0, cntfrq_el0":"=r"(f));if(!f)return 0;return(c/f)*1000000000ULL+((c%f)*1000000000ULL)/f;}
