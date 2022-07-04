// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#include "mmap32_bus.h"
#include "rtuart_bus.h"
#include "kernel.h"
#include <stdlib.h>

//struct rtuart_bus_ops * ops;
struct mmap32_bus
{
  struct rtuart_bus base_bus;
  void * base;
};

static int mmap32_read_u8(struct rtuart_bus * _bus, const int reg, u8 * value)
{
  struct mmap32_bus * bus = container_of(_bus, struct mmap32_bus, base_bus);
  *value = *(((u8*)bus->base)+4*reg);
  return 0;
}

static int mmap32_read_u16(struct rtuart_bus * _bus, const int reg, u16 * value)
{
  struct mmap32_bus * bus = container_of(_bus, struct mmap32_bus, base_bus);
  *value = *(((u16*)bus->base)+2*reg);
  return 0;
}

static int mmap32_read_u32(struct rtuart_bus * _bus, const int reg, u32 * value)
{
  struct mmap32_bus * bus = container_of(_bus, struct mmap32_bus, base_bus);
  *value = *(((u32*)bus->base)+reg);
  return 0;
}

static int mmap32_write_u8(struct rtuart_bus * _bus, const int reg, const u8 value)
{
  struct mmap32_bus * bus = container_of(_bus, struct mmap32_bus, base_bus);
  *(((u8*)bus->base)+4*reg) = value;
  return 0;
}

static int mmap32_write_u16(struct rtuart_bus * _bus, const int reg, const u16 value)
{
  struct mmap32_bus * bus = container_of(_bus, struct mmap32_bus, base_bus);
  *(((u16*)bus->base)+2*reg) = value;
  return 0;
}


static int mmap32_write_u32(struct rtuart_bus * _bus, const int reg, const u32 value)
{
  struct mmap32_bus * bus = container_of(_bus, struct mmap32_bus, base_bus);
  *(((u32*)bus->base)+reg) = value;
  return 0;
}

int mmap32_cleanup(struct rtuart_bus * _bus)
{
  struct mmap32_bus * bus = container_of(_bus, struct mmap32_bus, base_bus);
  free(bus);
}

struct rtuart_bus_ops uart_mmap32_bus_ops =
{
  .read_u8   = mmap32_read_u8,
  .write_u8  = mmap32_write_u8,
  .read_u16  = mmap32_read_u16,
  .write_u16 = mmap32_write_u16,
  .read_u32  = mmap32_read_u32,
  .write_u32 = mmap32_write_u32,
  .cleanup   = mmap32_cleanup
  // may have read_u64, write_u64 in the future.
};

struct rtuart_bus * mmap32_create(void * base)
{
  struct mmap32_bus * bus = (struct mmap32_bus*)malloc(sizeof(struct mmap32_bus));
  if (bus)
    {
      bus->base_bus.ops = &uart_mmap32_bus_ops;
      bus->base = base;
    }
  return &bus->base_bus;
}
