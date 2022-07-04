// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#include "uio32_bus.h"
#include "rtuart_bus.h"
#include "rtuart_bus_ops.h"
#include "kernel.h"
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>

//struct rtuart_bus_ops * ops;
struct uio32_bus
{
  struct rtuart_bus base_bus;
  int    fd;
  size_t size;
  u32 *  regs;
};

static int uio32_read_u8(struct rtuart_bus * _bus, const int reg, u8 * value)
{
  struct uio32_bus * bus = container_of(_bus, struct uio32_bus, base_bus);
  *value = *((u8*)(&bus->regs[reg]));
  return 0;
}

static int uio32_read_u16(struct rtuart_bus * _bus, const int reg, u16 * value)
{
  struct uio32_bus * bus = container_of(_bus, struct uio32_bus, base_bus);
  *value = *((u16*)(&bus->regs[reg]));
  return 0;
}

static int uio32_read_u32(struct rtuart_bus * _bus, const int reg, u32 * value)
{
  struct uio32_bus * bus = container_of(_bus, struct uio32_bus, base_bus);
  *value = *((u32*)(&bus->regs[reg]));
  return 0;
}

static int uio32_write_u8(struct rtuart_bus * _bus, const int reg, const u8 value)
{
  struct uio32_bus * bus = container_of(_bus, struct uio32_bus, base_bus);
  *((u8*)(&bus->regs[reg])) = value;
  return 0;
}

static int uio32_write_u16(struct rtuart_bus * _bus, const int reg, const u16 value)
{
  struct uio32_bus * bus = container_of(_bus, struct uio32_bus, base_bus);
  *((u16*)(&bus->regs[reg])) = value;
  return 0;
}


static int uio32_write_u32(struct rtuart_bus * _bus, const int reg, const u32 value)
{
  struct uio32_bus * bus = container_of(_bus, struct uio32_bus, base_bus);
  *((u32*)(&bus->regs[reg])) = value;
  return 0;
}

static int uio32_cleanup(struct rtuart_bus * _bus)
{
  struct uio32_bus * bus = container_of(_bus, struct uio32_bus, base_bus);
  // close(bus->fd);
  free(bus);
}

struct rtuart_bus_ops uart_uio32_bus_ops =
{
  .read_u8   = uio32_read_u8,
  .write_u8  = uio32_write_u8,
  .read_u16  = uio32_read_u16,
  .write_u16 = uio32_write_u16,
  .read_u32  = uio32_read_u32,
  .write_u32 = uio32_write_u32,
  .cleanup   = uio32_cleanup
  // may have read_u64, write_u64 in the future.
};

struct rtuart_bus * uio32_create(int fd, const int offset, const size_t size)
{
  struct uio32_bus * bus = 0;
  void * mem = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mem == MAP_FAILED)
    {
      perror("mmap failed\n");
      return 0;
    }

  bus = (struct uio32_bus*)malloc(sizeof(struct uio32_bus));
  if (bus)
    {
      bus->base_bus.ops = &uart_uio32_bus_ops;
      bus->fd = fd;
      bus->size = size;
      bus->regs = offset + mem;
    }
  return &bus->base_bus;
}

