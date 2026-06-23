/* SPDX-License-Identifier: Proprietary */
#pragma once
#include "../../kernel/include/types.h"

int  initramfs_mount(const void *data, u64 size);
int  initramfs_mount_empty_root(void);
bool initramfs_is_mounted(void);
u32  initramfs_file_count(void);

int  initramfs_install_builtin_userland(void);
bool initramfs_has_file(const char *path);
u64  initramfs_file_size(const char *path);
const char *initramfs_file_name(u32 index);
bool initramfs_file_is_dir(u32 index);
int  initramfs_read_text_file(const char *path, const char **out_data, u64 *out_size);
int  initramfs_install_release_rootfs(void);
