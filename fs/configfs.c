/* SPDX-License-Identifier: Proprietary */
/* AegisOS — fs/configfs.c */
#include "include/configfs.h"

int configfs_mount(const char *path) { (void)path; return 0; }
int configfs_get(const char *key, char *value, u32 max_len) {
    (void)key; (void)value; (void)max_len; return -1;
}
int configfs_set(const char *key, const char *value) {
    (void)key; (void)value; return 0;
}
