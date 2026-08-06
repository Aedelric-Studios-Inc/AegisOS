# AegisOS Makefile
# Builds the kernel, HAL, Rust userland, Aegis services, and rootfs staging for AArch64.

ARCH        := aarch64
CROSS       := aarch64-linux-gnu-
CC          := $(CROSS)gcc
AS          := $(CROSS)as
LD          := $(CROSS)ld
OBJCOPY     := $(CROSS)objcopy
CARGO       := cargo
RUST_TARGET ?= aarch64-unknown-linux-musl
SECURITY_TARGET ?= aarch64-unknown-none
NATIVE_USER_IMPL ?= assembly
EXTRA_LDFLAGS ?=

# Default board — override with: make BOARD=aegisbox-pro
BOARD       ?= bastion

CFLAGS      := -Wall -Wextra -ffreestanding -nostdlib \
               -fno-stack-protector -fno-PIE -mgeneral-regs-only -mstrict-align \
               -I kernel/include -I hal/include -I fs/include -I net/include \
               -I $(BUILD_DIR)/generated \
               -DBOARD_$(shell echo $(BOARD) | tr '[:lower:]' '[:upper:]' | tr - _) \
               $(EXTRA_CFLAGS) $(if $(filter rust,$(NATIVE_USER_IMPL)),-DAEGISOS_NATIVE_RUST_FULL=1,)

LDSCRIPT    := kernel/linker/aarch64-kernel.ld
BUILD_DIR   := build
ROOTFS_DIR  := image/rootfs
RUST_RELEASE := target/$(RUST_TARGET)/release

NATIVE_USER_DIR       := userland/native
NATIVE_USER_BUILD_DIR := $(BUILD_DIR)/userland/native
NATIVE_INIT_OBJ       := $(NATIVE_USER_BUILD_DIR)/aegis-init.o
NATIVE_INIT_ELF       := $(NATIVE_USER_BUILD_DIR)/aegis-init.elf
NATIVE_INIT_HEADER    := $(BUILD_DIR)/generated/aegis_init_elf.h
NATIVE_SERVICE_MANAGER_OBJ    := $(NATIVE_USER_BUILD_DIR)/service-manager.o
NATIVE_SERVICE_MANAGER_ELF    := $(NATIVE_USER_BUILD_DIR)/service-manager.elf
NATIVE_SERVICE_MANAGER_HEADER := $(BUILD_DIR)/generated/aegis_service_manager_elf.h
NATIVE_AEGISD_OBJ       := $(NATIVE_USER_BUILD_DIR)/aegisd.o
NATIVE_AEGISD_ELF       := $(NATIVE_USER_BUILD_DIR)/aegisd.elf
NATIVE_AEGISD_HEADER    := $(BUILD_DIR)/generated/aegis_aegisd_elf.h
NATIVE_DASHBOARD_HEADER := $(BUILD_DIR)/generated/aegis_dashboard_elf.h
NATIVE_RUSTMYADMIN_HEADER := $(BUILD_DIR)/generated/aegis_rustmyadmin_elf.h
NATIVE_USER_CFLAGS      := -ffreestanding -nostdlib -fno-stack-protector -fno-PIE -mgeneral-regs-only $(EXTRA_CFLAGS)

# Collect all C and S sources used by the actual kernel image.
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

SERVICE_MANIFESTS := \
    services/cloudflare-tunnel/Cargo.toml \
    services/container-runner/Cargo.toml \
    services/device-discovery/Cargo.toml \
    services/dns-filter/Cargo.toml \
    services/firewall/Cargo.toml \
    services/intrusion-watch/Cargo.toml \
    services/radio-manager/Cargo.toml \
    services/traffic-monitor/Cargo.toml \
    services/vpn/Cargo.toml \
    services/web-hosting/Cargo.toml

.PHONY: all kernel native-userland rust rust-userland rust-services rust-system rust-admin rust-dashboard rust-security \
        rootfs image flash qemu debug clean clean-junk audit-dead-files host-stack-install host-stack-status host-stack-logs

all: kernel rust rootfs

kernel: native-userland $(BUILD_DIR)/aegisos.elf $(BUILD_DIR)/aegisos.bin

ifeq ($(NATIVE_USER_IMPL),rust)
NATIVE_RUST_HEADERS := $(NATIVE_INIT_HEADER) $(NATIVE_SERVICE_MANAGER_HEADER) \
                       $(NATIVE_AEGISD_HEADER) $(NATIVE_DASHBOARD_HEADER) \
                       $(NATIVE_RUSTMYADMIN_HEADER)

native-userland:
	BUILD_DIR="$(BUILD_DIR)" tools/build/native-rust-aarch64.sh

# The Rust builder owns these generated headers.  The old unconditional
# assembly rules rebuilt the same filenames afterwards and silently embedded
# the assembly bootstrap ELFs into a kernel that claimed to be Rust-backed.
# Tie every generated header to the single Rust build and only validate it.
$(NATIVE_RUST_HEADERS): native-userland
	@test -s "$@" || { echo "error: missing Rust-generated native header: $@" >&2; exit 2; }
else
native-userland: $(NATIVE_INIT_ELF) $(NATIVE_INIT_HEADER) $(NATIVE_SERVICE_MANAGER_ELF) $(NATIVE_SERVICE_MANAGER_HEADER) $(NATIVE_AEGISD_ELF) $(NATIVE_AEGISD_HEADER)

$(NATIVE_INIT_OBJ): $(NATIVE_USER_DIR)/aegis-init.S
	@mkdir -p $(dir $@)
	$(CC) $(NATIVE_USER_CFLAGS) -c $< -o $@

$(NATIVE_INIT_ELF): $(NATIVE_INIT_OBJ) $(NATIVE_USER_DIR)/aegis-user.ld
	@mkdir -p $(dir $@)
	$(LD) -T $(NATIVE_USER_DIR)/aegis-user.ld -z max-page-size=4096 -o $@ $(NATIVE_INIT_OBJ)

$(NATIVE_INIT_HEADER): $(NATIVE_INIT_ELF) tools/build/embed-binary.py
	@mkdir -p $(dir $@)
	python3 tools/build/embed-binary.py $(NATIVE_INIT_ELF) $@ aegis_native_init_elf

$(NATIVE_SERVICE_MANAGER_OBJ): $(NATIVE_USER_DIR)/service-manager.S
	@mkdir -p $(dir $@)
	$(CC) $(NATIVE_USER_CFLAGS) -c $< -o $@

$(NATIVE_SERVICE_MANAGER_ELF): $(NATIVE_SERVICE_MANAGER_OBJ) $(NATIVE_USER_DIR)/aegis-user.ld
	@mkdir -p $(dir $@)
	$(LD) -T $(NATIVE_USER_DIR)/aegis-user.ld -z max-page-size=4096 -o $@ $(NATIVE_SERVICE_MANAGER_OBJ)

$(NATIVE_SERVICE_MANAGER_HEADER): $(NATIVE_SERVICE_MANAGER_ELF) tools/build/embed-binary.py
	@mkdir -p $(dir $@)
	python3 tools/build/embed-binary.py $(NATIVE_SERVICE_MANAGER_ELF) $@ aegis_native_service_manager_elf

$(NATIVE_AEGISD_OBJ): $(NATIVE_USER_DIR)/aegisd.S
	@mkdir -p $(dir $@)
	$(CC) $(NATIVE_USER_CFLAGS) -c $< -o $@

$(NATIVE_AEGISD_ELF): $(NATIVE_AEGISD_OBJ) $(NATIVE_USER_DIR)/aegis-user.ld
	@mkdir -p $(dir $@)
	$(LD) -T $(NATIVE_USER_DIR)/aegis-user.ld -z max-page-size=4096 -o $@ $(NATIVE_AEGISD_OBJ)

$(NATIVE_AEGISD_HEADER): $(NATIVE_AEGISD_ELF) tools/build/embed-binary.py
	@mkdir -p $(dir $@)
	python3 tools/build/embed-binary.py $(NATIVE_AEGISD_ELF) $@ aegis_native_aegisd_elf
endif

ifeq ($(NATIVE_USER_IMPL),rust)
$(BUILD_DIR)/fs/initramfs.o: $(NATIVE_RUST_HEADERS)
else
$(BUILD_DIR)/fs/initramfs.o: $(NATIVE_INIT_HEADER) $(NATIVE_SERVICE_MANAGER_HEADER) $(NATIVE_AEGISD_HEADER)
endif

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/aegisos.elf: $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(LD) $(EXTRA_LDFLAGS) -T $(LDSCRIPT) -o $@ $^

$(BUILD_DIR)/aegisos.bin: $(BUILD_DIR)/aegisos.elf
	$(OBJCOPY) -O binary $< $@

rust: rust-userland rust-security rust-dashboard rust-system rust-admin rust-services

rust-userland:
	$(CARGO) build --release --manifest-path userland/Cargo.toml --target $(RUST_TARGET)

rust-security:
	$(CARGO) build --release --manifest-path security/Cargo.toml --target $(SECURITY_TARGET)

rust-dashboard:
	$(CARGO) build --release --manifest-path dashboard/Cargo.toml --target $(RUST_TARGET)

rust-system:
	$(CARGO) build --release --manifest-path system/daemon/aegisd/Cargo.toml --target $(RUST_TARGET)
	$(CARGO) build --release --manifest-path system/cli/aegisctl/Cargo.toml --target $(RUST_TARGET)

rust-admin:
	$(CARGO) build --release --manifest-path rustmyadmin/Cargo.toml --target $(RUST_TARGET)

rust-services:
	@set -e; for manifest in $(SERVICE_MANIFESTS); do \
		echo "== cargo build $$manifest"; \
		$(CARGO) build --release --manifest-path $$manifest --target $(RUST_TARGET); \
	done

rootfs: rust
	@mkdir -p image/rootfs/sbin image/rootfs/bin image/rootfs/usr/sbin image/rootfs/usr/bin image/rootfs/etc image/rootfs/var/lib/rustmyadmin

	install -m 0755 userland/target/aarch64-unknown-linux-musl/release/aegis-init              image/rootfs/sbin/aegis-init
	install -m 0755 userland/target/aarch64-unknown-linux-musl/release/service-manager         image/rootfs/sbin/service-manager
	install -m 0755 userland/target/aarch64-unknown-linux-musl/release/aegis-shell             image/rootfs/bin/aegis-shell
	install -m 0755 userland/target/aarch64-unknown-linux-musl/release/netctl                  image/rootfs/bin/netctl
	install -m 0755 userland/target/aarch64-unknown-linux-musl/release/update-manager          image/rootfs/sbin/update-manager
	install -m 0755 userland/target/aarch64-unknown-linux-musl/release/aegis-diagnostics       image/rootfs/bin/aegis-diagnostics
	install -m 0755 userland/target/aarch64-unknown-linux-musl/release/aegis-recovery          image/rootfs/sbin/aegis-recovery
	install -m 0755 userland/target/aarch64-unknown-linux-musl/release/aegisctl                image/rootfs/bin/aegisctl

	install -m 0755 dashboard/target/aarch64-unknown-linux-musl/release/aegis-dashboard        image/rootfs/usr/sbin/aegis-dashboard
	install -m 0755 system/daemon/aegisd/target/aarch64-unknown-linux-musl/release/aegisd      image/rootfs/sbin/aegisd
	install -m 0755 system/cli/aegisctl/target/aarch64-unknown-linux-musl/release/system-aegisctl image/rootfs/usr/bin/system-aegisctl
	install -m 0755 rustmyadmin/target/aarch64-unknown-linux-musl/release/rustmyadmin          image/rootfs/usr/sbin/rustmyadmin

	install -m 0755 services/cloudflare-tunnel/target/aarch64-unknown-linux-musl/release/aegis-cloudflare-tunnel image/rootfs/usr/sbin/aegis-cloudflare-tunnel
	install -m 0755 services/container-runner/target/aarch64-unknown-linux-musl/release/aegis-container-runner     image/rootfs/usr/sbin/aegis-container-runner
	install -m 0755 services/device-discovery/target/aarch64-unknown-linux-musl/release/aegis-device-discovery     image/rootfs/usr/sbin/aegis-device-discovery
	install -m 0755 services/dns-filter/target/aarch64-unknown-linux-musl/release/aegis-dns-filter                 image/rootfs/usr/sbin/aegis-dns-filter
	install -m 0755 services/firewall/target/aarch64-unknown-linux-musl/release/aegis-firewall                     image/rootfs/usr/sbin/aegis-firewall
	install -m 0755 services/intrusion-watch/target/aarch64-unknown-linux-musl/release/aegis-intrusion-watch       image/rootfs/usr/sbin/aegis-intrusion-watch
	install -m 0755 services/radio-manager/target/aarch64-unknown-linux-musl/release/aegis-radio-manager           image/rootfs/usr/sbin/aegis-radio-manager
	install -m 0755 services/traffic-monitor/target/aarch64-unknown-linux-musl/release/aegis-traffic-monitor       image/rootfs/usr/sbin/aegis-traffic-monitor
	install -m 0755 services/vpn/target/aarch64-unknown-linux-musl/release/aegis-vpn                               image/rootfs/usr/sbin/aegis-vpn
	install -m 0755 services/web-hosting/target/aarch64-unknown-linux-musl/release/aegis-web-hosting               image/rootfs/usr/sbin/aegis-web-hosting

	install -m 0644 etc/services.toml image/rootfs/etc/services.toml

image: kernel rootfs
	@bash image/build_image.sh

flash: image
	@bash tools/flash/flash_aegisbox.sh

qemu: kernel
	@bash tools/qemu/run-aarch64.sh $(BUILD_DIR)/aegisos.bin

debug: kernel
	@bash tools/qemu/debug-aarch64.sh $(BUILD_DIR)/aegisos.bin

clean:
	rm -rf $(BUILD_DIR)
	$(CARGO) clean --manifest-path userland/Cargo.toml || true
	$(CARGO) clean --manifest-path security/Cargo.toml || true
	$(CARGO) clean --manifest-path dashboard/Cargo.toml || true
	$(CARGO) clean --manifest-path system/daemon/aegisd/Cargo.toml || true
	$(CARGO) clean --manifest-path system/cli/aegisctl/Cargo.toml || true
	$(CARGO) clean --manifest-path rustmyadmin/Cargo.toml || true
	@set -e; for manifest in $(SERVICE_MANIFESTS); do $(CARGO) clean --manifest-path $$manifest || true; done

clean-junk:
	rm -rf build release
	find . -type d -name __pycache__ -prune -exec rm -rf {} +
	find . -type f \( -name '*.pyc' -o -name '*.o' -o -name '*.elf' -o -name '*.bin' -o -name '*.img' \) -delete

audit-dead-files:
	@python3 tools/dev/audit-dead-files.py


host-stack-install:
	@bash ./tools/dev/install-aegisos-host-stack.sh

host-stack-status:
	@bash ./tools/dev/aegisos-host-stackctl.sh status

host-stack-logs:
	@bash ./tools/dev/aegisos-host-stackctl.sh logs
