/* SPDX-License-Identifier: Proprietary */
/* AegisOS — hal/drivers/ethernet/ethernet.c
 * Generic Ethernet driver stub.
 */

#include "../../include/ethernet.h"

static u8 mac_addr[ETH_ALEN] = {0x02, 0xAE, 0x61, 0x5B, 0x0X, 0x01};

int ethernet_init(void) {
    /* Board-specific PHY/MAC initialization would go here */
    return 0;
}

int ethernet_send(const u8 *frame, u16 len) {
    if (!frame || len == 0 || len > ETH_MTU + 14) return -1;
    /* DMA descriptor setup and transmit — TODO */
    (void)frame; (void)len;
    return 0;
}

int ethernet_recv(u8 *buf, u16 *len) {
    /* Poll RX DMA ring — TODO */
    (void)buf; (void)len;
    return -1;
}

void ethernet_get_mac(eth_addr_t *out) {
    for (int i = 0; i < ETH_ALEN; i++)
        out->mac[i] = mac_addr[i];
}
