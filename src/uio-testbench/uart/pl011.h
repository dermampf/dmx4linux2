// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#ifndef DEFINED_PL011
#define DEFINED_PL011

struct rtuart;
struct rtuart * pl011_create(const unsigned long base_clock);

#endif
