// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#ifndef DEFINED_DUMMY_BUS
#define DEFINED_DUMMY_BUS

struct rtuart_bus;
struct rtuart_bus * dummy_bus_create(const char * arg);

#endif
