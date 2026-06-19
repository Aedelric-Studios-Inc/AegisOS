/* SPDX-License-Identifier: Proprietary */
/* AegisOS — hal/drivers/usb/usb_core.c
 * USB host controller core stub.
 */

#include "../../../kernel/include/types.h"

typedef struct usb_device {
    u8  address;
    u16 vendor_id;
    u16 product_id;
    u8  class_code;
} usb_device_t;

int usb_core_init(void) {
    /* Initialize USB host controller (xHCI/EHCI) — TODO */
    return 0;
}

int usb_enumerate(void) {
    /* Walk USB topology, assign addresses, read descriptors — TODO */
    return 0;
}

int usb_control_transfer(u8 dev_addr, u8 request_type, u8 request,
                         u16 value, u16 index, void *data, u16 length) {
    (void)dev_addr; (void)request_type; (void)request;
    (void)value; (void)index; (void)data; (void)length;
    return 0;
}
