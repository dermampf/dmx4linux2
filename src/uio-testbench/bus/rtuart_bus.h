// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#ifndef DEFINED_RTUART_BUS
#define DEFINED_RTUART_BUS

#include "kernel.h"

struct rtuart_bus_ops;
struct rtuart_bus
{
  struct rtuart_bus_ops * ops;
};

int rtuart_bus_read_u8(struct rtuart_bus * u, const int reg, u8 * value);
int rtuart_bus_read_u16(struct rtuart_bus * u, const int reg, u16 * value);
int rtuart_bus_read_u32(struct rtuart_bus * u, const int reg, u32 * value);
int rtuart_bus_write_u8(struct rtuart_bus * u, const int reg, const u8 value);
int rtuart_bus_write_u16(struct rtuart_bus * u, const int reg, const u16 value);
int rtuart_bus_write_u32(struct rtuart_bus * u, const int reg, const u32 value);
int rtuart_bus_irq_pending(struct rtuart_bus * u, unsigned long * irqmask);

#endif
