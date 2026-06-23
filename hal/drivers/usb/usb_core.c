/* SPDX-License-Identifier: Proprietary */
/* AegisOS — hal/drivers/usb/usb_core.c
 * USB host controller driver (xHCI-based).
 * Provides enumeration, control/bulk/interrupt transfers for
 * USB 2.0/3.0 devices on the AegisBox platform.
 */

#include "../../../kernel/include/types.h"
#include "../../../kernel/include/memory.h"

/* xHCI MMIO registers */
#define XHCI_BASE           0x0C000000UL
#define XHCI_CAPLENGTH      (*(volatile u8  *)(XHCI_BASE + 0x00))
#define XHCI_HCSPARAMS1     (*(volatile u32 *)(XHCI_BASE + 0x04))
#define XHCI_HCSPARAMS2     (*(volatile u32 *)(XHCI_BASE + 0x08))
#define XHCI_HCCPARAMS1     (*(volatile u32 *)(XHCI_BASE + 0x10))
#define XHCI_DBOFF          (*(volatile u32 *)(XHCI_BASE + 0x14))
#define XHCI_RTSOFF         (*(volatile u32 *)(XHCI_BASE + 0x18))

/* Operational registers (offset by CAPLENGTH) */
static volatile u32 *xhci_op_base;
#define XHCI_USBCMD         (xhci_op_base[0])
#define XHCI_USBSTS         (xhci_op_base[1])
#define XHCI_PAGESIZE       (xhci_op_base[2])
#define XHCI_DNCTRL         (xhci_op_base[5])
#define XHCI_CRCR_LO        (xhci_op_base[6])
#define XHCI_CRCR_HI        (xhci_op_base[7])
#define XHCI_DCBAAP_LO      (xhci_op_base[12])
#define XHCI_DCBAAP_HI      (xhci_op_base[13])
#define XHCI_CONFIG          (xhci_op_base[14])

/* USB command bits */
#define USBCMD_RUN       (1 << 0)
#define USBCMD_HCRST     (1 << 1)
#define USBCMD_INTE      (1 << 2)
#define USBSTS_HCH       (1 << 0)
#define USBSTS_CNR       (1 << 11)

#define MAX_USB_DEVICES  32
#define MAX_PORTS        16

typedef struct usb_device {
    u8  address;
    u16 vendor_id;
    u16 product_id;
    u8  class_code;
    u8  subclass;
    u8  protocol;
    u8  speed;       /* 0=LS, 1=FS, 2=HS, 3=SS */
    u8  port;
    bool present;
    u8  max_packet_size;
    u8  num_configurations;
    u8  num_interfaces;
} usb_device_t;

/* USB descriptor types */
#define USB_DESC_DEVICE     1
#define USB_DESC_CONFIG     2
#define USB_DESC_STRING     3
#define USB_DESC_INTERFACE  4
#define USB_DESC_ENDPOINT   5

/* USB request types */
#define USB_REQ_GET_DESC    6
#define USB_REQ_SET_ADDR    5
#define USB_REQ_SET_CONFIG  9
#define USB_REQ_GET_STATUS  0

/* Setup packet */
typedef struct {
    u8  bmRequestType;
    u8  bRequest;
    u16 wValue;
    u16 wIndex;
    u16 wLength;
} __attribute__((packed)) usb_setup_t;

/* Transfer Ring Buffer (TRB) */
typedef struct {
    u64 parameter;
    u32 status;
    u32 control;
} __attribute__((packed)) xhci_trb_t;

static usb_device_t usb_devices[MAX_USB_DEVICES];
static u8 next_address = 1;
static bool usb_initialized = false;

/* Device Context Base Address Array */
static u64 dcbaa[MAX_USB_DEVICES + 1] __attribute__((aligned(64)));

/* Command Ring */
static xhci_trb_t cmd_ring[256] __attribute__((aligned(64)));
static u32 cmd_ring_idx = 0;

/* Event Ring */
static xhci_trb_t event_ring[256] __attribute__((aligned(64)));

int usb_core_init(void) {
    /* Calculate operational register base */
    u8 cap_length = XHCI_CAPLENGTH;
    xhci_op_base = (volatile u32 *)(XHCI_BASE + cap_length);

    /* Wait for Controller Not Ready to clear */
    int timeout = 100000;
    while ((XHCI_USBSTS & USBSTS_CNR) && --timeout > 0);
    if (timeout == 0) return -1;

    /* Halt controller if running */
    if (!(XHCI_USBSTS & USBSTS_HCH)) {
        XHCI_USBCMD &= ~USBCMD_RUN;
        timeout = 100000;
        while (!(XHCI_USBSTS & USBSTS_HCH) && --timeout > 0);
    }

    /* Reset controller */
    XHCI_USBCMD = USBCMD_HCRST;
    timeout = 100000;
    while ((XHCI_USBCMD & USBCMD_HCRST) && --timeout > 0);
    while ((XHCI_USBSTS & USBSTS_CNR) && --timeout > 0);

    /* Get number of ports and device slots */
    u32 hcsparams1 = XHCI_HCSPARAMS1;
    u32 max_slots = hcsparams1 & 0xFF;
    if (max_slots > MAX_USB_DEVICES) max_slots = MAX_USB_DEVICES;

    /* Set max device slots */
    XHCI_CONFIG = max_slots;

    /* Setup DCBAA */
    for (int i = 0; i <= MAX_USB_DEVICES; i++) dcbaa[i] = 0;
    XHCI_DCBAAP_LO = (u32)((u64)(uptr)dcbaa & 0xFFFFFFFF);
    XHCI_DCBAAP_HI = (u32)((u64)(uptr)dcbaa >> 32);

    /* Setup command ring */
    for (int i = 0; i < 256; i++) {
        cmd_ring[i].parameter = 0;
        cmd_ring[i].status = 0;
        cmd_ring[i].control = 0;
    }
    /* Link TRB at end */
    cmd_ring[255].parameter = (u64)(uptr)cmd_ring;
    cmd_ring[255].control = (6 << 10) | (1 << 5); /* Link TRB + Toggle */

    XHCI_CRCR_LO = (u32)((u64)(uptr)cmd_ring & 0xFFFFFFFF) | 1; /* Ring cycle */
    XHCI_CRCR_HI = (u32)((u64)(uptr)cmd_ring >> 32);

    /* Initialize device array */
    for (int i = 0; i < MAX_USB_DEVICES; i++) {
        usb_devices[i].present = false;
    }

    /* Start controller */
    XHCI_USBCMD = USBCMD_RUN | USBCMD_INTE;
    timeout = 100000;
    while ((XHCI_USBSTS & USBSTS_HCH) && --timeout > 0);

    usb_initialized = true;
    return 0;
}

int usb_enumerate(void) {
    if (!usb_initialized) return -1;

    /* Scan ports for connected devices */
    u32 hcsparams1 = XHCI_HCSPARAMS1;
    u32 num_ports = (hcsparams1 >> 24) & 0xFF;
    if (num_ports > MAX_PORTS) num_ports = MAX_PORTS;

    u8 cap_length = XHCI_CAPLENGTH;
    volatile u32 *portsc_base = (volatile u32 *)(XHCI_BASE + cap_length + 0x400);

    for (u32 port = 0; port < num_ports; port++) {
        volatile u32 *portsc = &portsc_base[port * 4];
        u32 status = *portsc;

        /* Check Current Connect Status */
        if (!(status & 1)) continue;

        /* Port has device — assign address */
        if (next_address >= MAX_USB_DEVICES) break;

        usb_device_t *dev = &usb_devices[next_address - 1];
        dev->address = next_address++;
        dev->port = (u8)port;
        dev->present = true;

        /* Determine speed from Port Speed field */
        u8 speed = (status >> 10) & 0xF;
        dev->speed = speed;
        dev->max_packet_size = (speed >= 3) ? 64 : 8;

        /* In a full implementation, we would:
         * 1. Issue Enable Slot command on command ring
         * 2. Send SET_ADDRESS control transfer
         * 3. Read device descriptor via GET_DESCRIPTOR
         * 4. Parse VID/PID, class, etc.
         * 5. Set configuration
         */
        dev->vendor_id = 0;
        dev->product_id = 0;
        dev->class_code = 0;
    }

    return 0;
}

int usb_control_transfer(u8 dev_addr, u8 request_type, u8 request,
                         u16 value, u16 index, void *data, u16 length) {
    if (!usb_initialized) return -1;
    if (dev_addr == 0 || dev_addr >= MAX_USB_DEVICES) return -1;

    /* Build setup TRB */
    usb_setup_t setup = {
        .bmRequestType = request_type,
        .bRequest = request,
        .wValue = value,
        .wIndex = index,
        .wLength = length,
    };

    /* Enqueue Setup Stage TRB */
    xhci_trb_t *trb = &cmd_ring[cmd_ring_idx];
    trb->parameter = *(u64 *)&setup;
    trb->status = 8; /* TRB transfer length = 8 for setup */
    trb->control = (2 << 10) | (1 << 6) | 1; /* Setup Stage + IDT + Cycle */
    cmd_ring_idx = (cmd_ring_idx + 1) % 255;

    /* If data stage needed */
    if (length > 0 && data) {
        trb = &cmd_ring[cmd_ring_idx];
        trb->parameter = (u64)(uptr)data;
        trb->status = length;
        u32 dir = (request_type & 0x80) ? (1 << 16) : 0;
        trb->control = (3 << 10) | dir | 1; /* Data Stage + direction + Cycle */
        cmd_ring_idx = (cmd_ring_idx + 1) % 255;
    }

    /* Status Stage TRB */
    trb = &cmd_ring[cmd_ring_idx];
    trb->parameter = 0;
    trb->status = 0;
    u32 status_dir = (request_type & 0x80) ? 0 : (1 << 16);
    trb->control = (4 << 10) | status_dir | (1 << 5) | 1; /* Status + IOC + Cycle */
    cmd_ring_idx = (cmd_ring_idx + 1) % 255;

    /* Ring doorbell (device slot doorbell) */
    u32 db_off = XHCI_DBOFF;
    volatile u32 *doorbell = (volatile u32 *)(XHCI_BASE + db_off + dev_addr * 4);
    *doorbell = 1; /* Ring EP0 doorbell */

    /* Wait for completion event */
    int timeout = 100000;
    while (--timeout > 0) {
        if (event_ring[0].control & 1) {
            event_ring[0].control = 0;
            break;
        }
    }

    return (timeout > 0) ? 0 : -1;
}

usb_device_t *usb_get_device(u8 address) {
    if (address == 0 || address >= MAX_USB_DEVICES) return NULL;
    usb_device_t *dev = &usb_devices[address - 1];
    return dev->present ? dev : NULL;
}

int usb_get_device_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_USB_DEVICES; i++) {
        if (usb_devices[i].present) count++;
    }
    return count;
}
