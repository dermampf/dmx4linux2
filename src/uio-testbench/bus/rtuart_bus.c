// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#include "rtuart_bus.h"

#include "rtuart_bus_ops.h"
#include "kernel.h"

int rtuart_bus_read_u8(struct rtuart_bus * bus, const int reg, u8 * value)
{
  return bus->ops->read_u8 ? bus->ops->read_u8(bus, reg, value) : -1;
}

int rtuart_bus_read_u16(struct rtuart_bus * bus, const int reg, u16 * value)
{
  return bus->ops->read_u16 ? bus->ops->read_u16(bus, reg, value) : -1;
}

int rtuart_bus_read_u32(struct rtuart_bus * bus, const int reg, u32 * value)
{
  return bus->ops->read_u32 ? bus->ops->read_u32(bus, reg, value) : -1;
}

int rtuart_bus_write_u8(struct rtuart_bus * bus, const int reg, const u8 value)
{
  return bus->ops->write_u8 ? bus->ops->write_u8(bus, reg, value) : -1;
}

int rtuart_bus_write_u16(struct rtuart_bus * bus, const int reg, const u16 value)
{
  return bus->ops->write_u16 ? bus->ops->write_u16(bus, reg, value) : -1;
}

int rtuart_bus_write_u32(struct rtuart_bus * bus, const int reg, const u32 value)
{
  return bus->ops->write_u32 ? bus->ops->write_u32(bus, reg, value) : -1;
}
