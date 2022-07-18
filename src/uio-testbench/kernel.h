// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#ifndef DEFINED_KERNEL
#define DEFINED_KERNEL

typedef unsigned long  u32;
typedef unsigned short u16;
typedef unsigned char  u8;

#include <unistd.h>
#include "kern_levels.h"

int printk(const char * fmt, ...);
void set_loglevel(const int);

#undef offsetof
#ifdef __compiler_offsetof
#define offsetof(TYPE, MEMBER)  __compiler_offsetof(TYPE, MEMBER)
#else
#define offsetof(TYPE, MEMBER)  ((size_t)&((TYPE *)0)->MEMBER)
#endif

#define container_of(ptr, type, member) ({              \
        void *__mptr = (void *)(ptr);                   \
        ((type *)(__mptr - offsetof(type, member))); })

#endif
