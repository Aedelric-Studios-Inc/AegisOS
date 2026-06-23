/* SPDX-License-Identifier: Proprietary */
#pragma once
/* types.h — fundamental type definitions for AegisOS kernel */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef int8_t    s8;
typedef int16_t   s16;
typedef int32_t   s32;
typedef int64_t   s64;
typedef uintptr_t uptr;
typedef intptr_t  iptr;

typedef u64 phys_addr_t;
typedef u64 virt_addr_t;
typedef u64 cap_token_t;

#define NULL_CAP ((cap_token_t)0)

#define AEGIS_OK      (0)
#define AEGIS_ENOMEM  (-1)
#define AEGIS_EINVAL  (-2)
#define AEGIS_EPERM   (-3)
#define AEGIS_ENOENT  (-4)
#define AEGIS_EBUSY   (-5)
