// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#ifndef DEFINED_FPGASIM_BUS
#define DEFINED_FPGASIM_BUS

#include <unistd.h>
struct rtuart_bus * fpgasim_create(int argc, char ** argv, int irqfd);

#endif
