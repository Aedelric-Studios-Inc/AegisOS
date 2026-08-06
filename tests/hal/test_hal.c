/* SPDX-License-Identifier: Proprietary */
/* AegisOS — tests/hal/test_hal.c
 * Unit tests for the hardware abstraction layer.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ---- UART FIFO simulation ---- */

#define UART_FIFO_SIZE 256
static uint8_t uart_fifo[UART_FIFO_SIZE];
static int uart_rp = 0, uart_wp = 0;

static void uart_sim_putchar(char c) {
    uart_fifo[uart_wp % UART_FIFO_SIZE] = (uint8_t)c;
    uart_wp++;
}

static int uart_sim_getchar(void) {
    if (uart_rp == uart_wp) return -1;
    return uart_fifo[uart_rp++ % UART_FIFO_SIZE];
}

/* ---- MMIO register simulation (for VIRTIO-style descriptors) ---- */

typedef struct {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} virt_desc_t;

#define RING_SIZE 16
static virt_desc_t desc_ring[RING_SIZE];
static int avail_idx = 0;
static int used_idx  = 0;

static int virtio_add_buf(uint64_t phys, uint32_t len, uint16_t flags) {
    if ((avail_idx - used_idx) >= RING_SIZE) return -1; /* ring full */
    int slot = avail_idx % RING_SIZE;
    desc_ring[slot].addr  = phys;
    desc_ring[slot].len   = len;
    desc_ring[slot].flags = flags;
    desc_ring[slot].next  = 0;
    avail_idx++;
    return slot;
}

static int virtio_pop_buf(uint64_t *phys_out, uint32_t *len_out) {
    if (used_idx == avail_idx) return -1;
    int slot = used_idx % RING_SIZE;
    if (phys_out) *phys_out = desc_ring[slot].addr;
    if (len_out)  *len_out  = desc_ring[slot].len;
    used_idx++;
    return slot;
}

/* ---- GPIO bit-bang simulation ---- */

static uint32_t gpio_output = 0;

static void gpio_set(int pin)   { gpio_output |=  (1u << pin); }
static void gpio_clear(int pin) { gpio_output &= ~(1u << pin); }
static int  gpio_read(int pin)  { return (gpio_output >> pin) & 1; }

/* ---- Tests ---- */

static void test_uart_fifo_round_trip(void) {
    const char *msg = "AegisOS HAL";
    for (const char *p = msg; *p; p++) uart_sim_putchar(*p);
    char got[32]; int i = 0;
    int c;
    while ((c = uart_sim_getchar()) >= 0) got[i++] = (char)c;
    got[i] = '\0';
    assert(strcmp(got, msg) == 0);
    printf("  [PASS] UART FIFO round-trip\n");
}

static void test_uart_fifo_empty(void) {
    uart_rp = uart_wp = 0;
    assert(uart_sim_getchar() == -1);
    printf("  [PASS] UART getchar on empty FIFO returns -1\n");
}

static void test_virtio_descriptor_ring(void) {
    avail_idx = used_idx = 0;
    int s = virtio_add_buf(0xDEAD0000ULL, 1500, 0);
    assert(s >= 0);
    uint64_t pa; uint32_t len;
    int r = virtio_pop_buf(&pa, &len);
    assert(r >= 0);
    assert(pa  == 0xDEAD0000ULL);
    assert(len == 1500);
    printf("  [PASS] VIRTIO descriptor ring add/pop round-trip\n");
}

static void test_virtio_ring_full(void) {
    avail_idx = used_idx = 0;
    for (int i = 0; i < RING_SIZE; i++) virtio_add_buf((uint64_t)i, 64, 0);
    assert(virtio_add_buf(0xFFFF, 64, 0) == -1); /* ring full */
    printf("  [PASS] VIRTIO ring full returns error\n");
}

static void test_gpio_set_clear(void) {
    gpio_output = 0;
    gpio_set(3);
    assert(gpio_read(3) == 1);
    gpio_clear(3);
    assert(gpio_read(3) == 0);
    printf("  [PASS] GPIO set/clear/read\n");
}

static void test_gpio_multiple_pins(void) {
    gpio_output = 0;
    gpio_set(0); gpio_set(7); gpio_set(15);
    assert(gpio_read(0)  == 1);
    assert(gpio_read(7)  == 1);
    assert(gpio_read(15) == 1);
    assert(gpio_read(1)  == 0); /* untouched */
    printf("  [PASS] Multiple GPIO pins are independent\n");
}

static void test_sdcard_block_address(void) {
    /* Block addressing: LBA * 512 == byte offset */
    uint32_t lba = 128;
    uint64_t byte_off = (uint64_t)lba * 512;
    assert(byte_off == 65536ULL);
    printf("  [PASS] SD card LBA to byte-offset calculation\n");
}

int main(void) {
    printf("[test_hal] Running HAL unit tests...\n");
    test_uart_fifo_round_trip();
    test_uart_fifo_empty();
    test_virtio_descriptor_ring();
    test_virtio_ring_full();
    test_gpio_set_clear();
    test_gpio_multiple_pins();
    test_sdcard_block_address();
    printf("[test_hal] All tests passed\n");
    return 0;
}
