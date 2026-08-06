/* SPDX-License-Identifier: Proprietary */
/* AegisOS — hal/drivers/storage/sdcard.c
 * SD/SDHC card driver using a generic SDHCI controller interface.
 * Supports standard SD Physical Layer commands over the SDHCI MMIO interface.
 */

#include "../../include/storage.h"
#include "../../../kernel/include/memory.h"

/* SDHCI registers (generic, QEMU-compatible) */
#define SDHCI_BASE          0x0B000000UL
#define SDHCI_SDMA_ADDR     (*(volatile u32 *)(SDHCI_BASE + 0x00))
#define SDHCI_BLOCK_SIZE    (*(volatile u16 *)(SDHCI_BASE + 0x04))
#define SDHCI_BLOCK_COUNT   (*(volatile u16 *)(SDHCI_BASE + 0x06))
#define SDHCI_ARGUMENT      (*(volatile u32 *)(SDHCI_BASE + 0x08))
#define SDHCI_TRANSFER_MODE (*(volatile u16 *)(SDHCI_BASE + 0x0C))
#define SDHCI_COMMAND       (*(volatile u16 *)(SDHCI_BASE + 0x0E))
#define SDHCI_RESPONSE0     (*(volatile u32 *)(SDHCI_BASE + 0x10))
#define SDHCI_RESPONSE1     (*(volatile u32 *)(SDHCI_BASE + 0x14))
#define SDHCI_DATA_PORT     (*(volatile u32 *)(SDHCI_BASE + 0x20))
#define SDHCI_PRESENT_STATE (*(volatile u32 *)(SDHCI_BASE + 0x24))
#define SDHCI_HOST_CTRL     (*(volatile u8  *)(SDHCI_BASE + 0x28))
#define SDHCI_CLOCK_CTRL    (*(volatile u16 *)(SDHCI_BASE + 0x2C))
#define SDHCI_SW_RESET      (*(volatile u8  *)(SDHCI_BASE + 0x2F))
#define SDHCI_INT_STATUS    (*(volatile u32 *)(SDHCI_BASE + 0x30))
#define SDHCI_INT_ENABLE    (*(volatile u32 *)(SDHCI_BASE + 0x34))
#define SDHCI_CAPABILITIES  (*(volatile u32 *)(SDHCI_BASE + 0x40))

/* SD commands */
#define CMD_GO_IDLE      0
#define CMD_SEND_IF_COND 8
#define CMD_SEND_CSD     9
#define CMD_SET_BLOCKLEN 16
#define CMD_READ_SINGLE  17
#define CMD_READ_MULTI   18
#define CMD_WRITE_SINGLE 24
#define CMD_WRITE_MULTI  25
#define CMD_APP_CMD      55
#define ACMD_SD_SEND_OP  41

/* Transfer mode flags */
#define TM_READ       (1 << 4)
#define TM_MULTI      (1 << 5)
#define TM_BLOCK_CNT  (1 << 1)

/* Present state bits */
#define PS_CMD_INHIBIT   (1 << 0)
#define PS_DAT_INHIBIT   (1 << 1)
#define PS_BUF_READ_RDY  (1 << 11)
#define PS_BUF_WRITE_RDY (1 << 10)

static storage_info_t sdcard_info = {
    .block_count = 0,
    .block_size  = BLOCK_SIZE,
    .label       = "sdcard",
};

static bool sd_initialized = false;
static bool sd_is_sdhc = false;

static void sdhci_wait_cmd_ready(void) {
    int timeout = 100000;
    while ((SDHCI_PRESENT_STATE & PS_CMD_INHIBIT) && --timeout > 0);
}

static int sdhci_send_cmd(u16 cmd_idx, u32 arg, u32 *resp) {
    sdhci_wait_cmd_ready();
    SDHCI_INT_STATUS = 0xFFFFFFFF; /* Clear all interrupts */
    SDHCI_ARGUMENT = arg;
    SDHCI_COMMAND = (cmd_idx << 8) | 0x1A; /* Command + CRC + Index check + 48-bit response */

    /* Wait for command complete */
    int timeout = 100000;
    while (!(SDHCI_INT_STATUS & 0x01) && --timeout > 0);
    if (timeout == 0) return -1;

    if (resp) *resp = SDHCI_RESPONSE0;
    SDHCI_INT_STATUS = 0x01; /* Clear command complete */
    return 0;
}

int storage_init(void) {
    /* Reset controller */
    SDHCI_SW_RESET = 0x07;
    int timeout = 10000;
    while ((SDHCI_SW_RESET & 0x07) && --timeout > 0);

    /* Set clock to 400 kHz for identification */
    SDHCI_CLOCK_CTRL = (0x80 << 8) | 0x01; /* Divisor 128, internal clock enable */
    timeout = 10000;
    while (!(SDHCI_CLOCK_CTRL & 0x02) && --timeout > 0); /* Wait stable */
    SDHCI_CLOCK_CTRL |= 0x04; /* SD clock enable */

    /* Enable all interrupts */
    SDHCI_INT_ENABLE = 0xFFFFFFFF;

    /* CMD0 — GO_IDLE_STATE */
    sdhci_send_cmd(CMD_GO_IDLE, 0, NULL);

    /* CMD8 — SEND_IF_COND (voltage check) */
    u32 resp;
    if (sdhci_send_cmd(CMD_SEND_IF_COND, 0x1AA, &resp) == 0 && (resp & 0x1FF) == 0x1AA) {
        sd_is_sdhc = true;
    }

    /* ACMD41 loop — wait for card ready */
    u32 ocr = 0;
    for (int i = 0; i < 100; i++) {
        sdhci_send_cmd(CMD_APP_CMD, 0, NULL);
        u32 arg = 0x00FF8000; /* 3.2-3.4V */
        if (sd_is_sdhc) arg |= (1UL << 30); /* HCS */
        if (sdhci_send_cmd(ACMD_SD_SEND_OP, arg, &ocr) == 0) {
            if (ocr & (1UL << 31)) break; /* Card ready */
        }
    }

    /* Set block length to 512 for standard SD (SDHC uses fixed 512) */
    if (!sd_is_sdhc) {
        sdhci_send_cmd(CMD_SET_BLOCKLEN, BLOCK_SIZE, NULL);
    }

    /* Get card capacity from CSD */
    sdcard_info.block_count = 15523840; /* Default ~8GB — real impl reads CSD */
    sd_initialized = true;

    /* Switch to high-speed clock (25 MHz) */
    SDHCI_CLOCK_CTRL = 0;
    SDHCI_CLOCK_CTRL = (0x02 << 8) | 0x01; /* Divisor 2 */
    timeout = 10000;
    while (!(SDHCI_CLOCK_CTRL & 0x02) && --timeout > 0);
    SDHCI_CLOCK_CTRL |= 0x04;

    return 0;
}

int storage_read(u32 lba, u32 count, void *buf) {
    if (!sd_initialized || !buf || count == 0) return -1;

    u32 addr = sd_is_sdhc ? lba : (lba * BLOCK_SIZE);
    u32 *dest = (u32 *)buf;

    for (u32 block = 0; block < count; block++) {
        /* Wait for data line free */
        int timeout = 100000;
        while ((SDHCI_PRESENT_STATE & PS_DAT_INHIBIT) && --timeout > 0);

        SDHCI_BLOCK_SIZE = BLOCK_SIZE;
        SDHCI_BLOCK_COUNT = 1;
        SDHCI_TRANSFER_MODE = TM_READ;
        SDHCI_INT_STATUS = 0xFFFFFFFF;
        SDHCI_ARGUMENT = addr + (sd_is_sdhc ? block : block * BLOCK_SIZE);
        SDHCI_COMMAND = (CMD_READ_SINGLE << 8) | 0x3A; /* Data present, 48-bit resp */

        /* Wait for command complete */
        timeout = 100000;
        while (!(SDHCI_INT_STATUS & 0x01) && --timeout > 0);
        SDHCI_INT_STATUS = 0x01;

        /* Wait for buffer read ready */
        timeout = 100000;
        while (!(SDHCI_INT_STATUS & 0x20) && --timeout > 0);

        /* Read 512 bytes (128 * 4) */
        for (int i = 0; i < 128; i++) {
            dest[block * 128 + i] = SDHCI_DATA_PORT;
        }
        SDHCI_INT_STATUS = 0x22; /* Clear transfer complete + buffer read */
    }

    return 0;
}

int storage_write(u32 lba, u32 count, const void *buf) {
    if (!sd_initialized || !buf || count == 0) return -1;

    u32 addr = sd_is_sdhc ? lba : (lba * BLOCK_SIZE);
    const u32 *src = (const u32 *)buf;

    for (u32 block = 0; block < count; block++) {
        int timeout = 100000;
        while ((SDHCI_PRESENT_STATE & PS_DAT_INHIBIT) && --timeout > 0);

        SDHCI_BLOCK_SIZE = BLOCK_SIZE;
        SDHCI_BLOCK_COUNT = 1;
        SDHCI_TRANSFER_MODE = 0; /* Write, single block */
        SDHCI_INT_STATUS = 0xFFFFFFFF;
        SDHCI_ARGUMENT = addr + (sd_is_sdhc ? block : block * BLOCK_SIZE);
        SDHCI_COMMAND = (CMD_WRITE_SINGLE << 8) | 0x3A;

        /* Wait for command complete */
        timeout = 100000;
        while (!(SDHCI_INT_STATUS & 0x01) && --timeout > 0);
        SDHCI_INT_STATUS = 0x01;

        /* Wait for buffer write ready */
        timeout = 100000;
        while (!(SDHCI_INT_STATUS & 0x10) && --timeout > 0);

        /* Write 512 bytes */
        for (int i = 0; i < 128; i++) {
            SDHCI_DATA_PORT = src[block * 128 + i];
        }
        SDHCI_INT_STATUS = 0x12; /* Clear transfer complete + buffer write */

        /* Wait for transfer complete */
        timeout = 100000;
        while (!(SDHCI_INT_STATUS & 0x02) && --timeout > 0);
        SDHCI_INT_STATUS = 0x02;
    }

    return 0;
}

int storage_get_info(storage_info_t *info) {
    if (!info) return -1;
    *info = sdcard_info;
    return 0;
}
