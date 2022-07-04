// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#ifndef DEFINED_MMAP32_BUS
#define DEFINED_MMAP32_BUS

#include <unistd.h>
struct rtuart_bus * uio32_create(int fd, const int offset, const size_t _size);

#endif
