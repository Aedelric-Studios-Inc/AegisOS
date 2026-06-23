# AegisOS Makefile
# Builds the kernel, HAL, and Rust userland/services for AArch64.

ARCH        := aarch64
CROSS       := aarch64-linux-gnu-
CC          := $(CROSS)gcc
AS          := $(CROSS)as
LD          := $(CROSS)ld
OBJCOPY     := $(CROSS)objcopy
CARGO       := cargo

# Default board — override with: make BOARD=aegisbox-pro
BOARD       ?= bastion

CFLAGS      := -Wall -Wextra -ffreestanding -nostdlib \
               -fno-stack-protector -fno-PIE -mgeneral-regs-only \
               -I kernel/include -I hal/include -I fs/include -I net/include \
               -DBOARD_$(shell echo $(BOARD) | tr '[:lower:]' '[:upper:]' | tr - _) \
               $(EXTRA_CFLAGS)

LDSCRIPT    := kernel/linker/aarch64-kernel.ld
BUILD_DIR   := build

# Collect all C and S sources
KERNEL_SRCS := $(wildcard kernel/core/*.c) \
               $(wildcard kernel/memory/*.c) \
               $(wildcard kernel/ipc/*.c)

HAL_SRCS    := $(wildcard hal/arm64/*.c) \
               $(wildcard hal/drivers/uart/*.c) \
               $(wildcard hal/drivers/gpio/*.c) \
               $(wildcard hal/drivers/ethernet/*.c) \
               $(wildcard hal/drivers/storage/*.c) \
               $(wildcard hal/drivers/usb/*.c) \
               $(wildcard hal/drivers/watchdog/*.c) \
               hal/boards/$(BOARD)/board.c

NET_SRCS    := $(wildcard net/*.c)
FS_SRCS     := $(wildcard fs/*.c)

ASM_SRCS    := $(wildcard boot/arm64/*.S)

ALL_SRCS    := $(KERNEL_SRCS) $(HAL_SRCS) $(NET_SRCS) $(FS_SRCS)
OBJS        := $(patsubst %.c,$(BUILD_DIR)/%.o,$(ALL_SRCS)) \
               $(patsubst %.S,$(BUILD_DIR)/%.o,$(ASM_SRCS))

.PHONY: all kernel rust clean flash qemu

all: kernel rust

kernel: $(BUILD_DIR)/aegisos.elf $(BUILD_DIR)/aegisos.bin

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/aegisos.elf: $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(LD) -T $(LDSCRIPT) -o $@ $^

$(BUILD_DIR)/aegisos.bin: $(BUILD_DIR)/aegisos.elf
	$(OBJCOPY) -O binary $< $@

rust:
	$(CARGO) build --release --manifest-path userland/Cargo.toml \
	    --target aarch64-unknown-linux-musl
	$(CARGO) build --release --manifest-path security/Cargo.toml \
	    --target aarch64-unknown-none
	$(CARGO) build --release --manifest-path dashboard/Cargo.toml \
	    --target aarch64-unknown-linux-musl

image: kernel
	@bash image/build_image.sh

flash: image
	@bash tools/flash/flash_aegisbox.sh

qemu: kernel
	@bash tools/qemu/run-aarch64.sh $(BUILD_DIR)/aegisos.bin

debug: kernel
	@bash tools/qemu/debug-aarch64.sh $(BUILD_DIR)/aegisos.bin

clean:
	rm -rf $(BUILD_DIR)
	$(CARGO) clean --manifest-path userland/Cargo.toml
	$(CARGO) clean --manifest-path security/Cargo.toml
	$(CARGO) clean --manifest-path dashboard/Cargo.toml
