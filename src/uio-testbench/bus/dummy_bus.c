// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#include "dummy_bus.h"
#include "rtuart_bus.h"
#include "rtuart_bus_ops.h"
#include "kernel.h"
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>

struct dummy_bus
{
    struct rtuart_bus base_bus;
    u8 register_values[0x3f];
};

enum
{
  REGNO_RBR = 0, // read only
  REGNO_THR = 0, // write only
  REGNO_IER = 1, // read/write
  REGNO_IIR = 2, // read only
  REGNO_FCR = 8+2, // write only
  REGNO_LCR = 3, // read/write
  REGNO_MCR = 4, // read/write
  REGNO_LSR = 5, // read/write
  REGNO_MSR = 6, // read/write
  REGNO_SCRATCH = 7, // read/write
  REGNO_DLAB_LSB = 8+0, // read/write
  REGNO_DLAB_MSB = 8+1, // read/write
};

static const char * regno2name(const int regno)
{
  switch(regno)
    {
      // case REGNO_RBR = 0, // read only
    case REGNO_THR: return "THR";
    case REGNO_IER: return "IER";
    case REGNO_IIR: return "IIR";
    case REGNO_FCR: return "FCR";
    case REGNO_LCR: return "LCR";
    case REGNO_MCR: return "MCR";
    case REGNO_LSR: return "LSR";
    case REGNO_MSR: return "MSR";
    case REGNO_SCRATCH: return "SCRATCH";
    case REGNO_DLAB_LSB: return "DLAB_LSB";
    case REGNO_DLAB_MSB: return "DLAB_MSB";
    default:
      break;
    }
  return "???";
};

static void dummy_bus_init_internal_registers(u8 * regs)
{
  regs[REGNO_RBR] = 0;
  regs[REGNO_THR] = 0;
  regs[REGNO_IER] = 0;
  regs[REGNO_IIR] = 1;
  regs[REGNO_FCR] = 0;
  regs[REGNO_LCR] = 0;
  regs[REGNO_MCR] = 0;
  regs[REGNO_LSR] = 0x60;
  regs[REGNO_MSR] = 0;
  regs[REGNO_SCRATCH] = 0;
  regs[REGNO_DLAB_LSB] = 0;
  regs[REGNO_DLAB_MSB] = 0;
}

static u8 dummy_bus_read_internal_reg(u8 * regs, const int regno)
{
  u8 value = 0;
  switch(regno)
    {
    case REGNO_RBR:
      value = regs[regno];
      break;
    default:
      value = regs[regno];
      break;
    }
  printf ("read_reg(reg:%s) => value:%02X\n",
	  regno2name(regno), value);
  return value;
}

static void dummy_bus_write_internal_reg(u8 * regs, const int regno, const u8 value)
{
  printf ("write_reg(reg:%s, %04X\n",
	  regno2name(regno), value);
  switch(regno)
    {
    case REGNO_THR:
      regs[regno] = value;
      break;

    case REGNO_IIR: // IIR is read only. Writes go to FCR.
      regs[REGNO_FCR] = value;
      break;

    default:
      regs[regno] = value;
      break;
    }
}



static inline u8 dummy_bus_read_reg(struct dummy_bus * bus, const int regno)
{
  const int dlab = bus->register_values[REGNO_LCR] & 0x80;
  if (dlab && (regno <= 1))
    return dummy_bus_read_internal_reg(bus->register_values, regno + 8);
  return dummy_bus_read_internal_reg(bus->register_values, regno);
}

static inline void dummy_bus_write_reg(struct dummy_bus * bus, const int regno, const u8 value)
{
  const int dlab = bus->register_values[REGNO_LCR] & 0x80;
  if (dlab && (regno <= 1))
    dummy_bus_write_internal_reg(bus->register_values, regno + 8, value);
  else
    dummy_bus_write_internal_reg(bus->register_values, regno, value);
}


static int dummy_read_u8(struct rtuart_bus * _bus, const int reg, u8 * value)
{
  struct dummy_bus * bus = container_of(_bus, struct dummy_bus, base_bus);
  *value = dummy_bus_read_reg(bus, reg);
  return 0;
}

static int dummy_read_u16(struct rtuart_bus * _bus, const int reg, u16 * value)
{
  struct dummy_bus * bus = container_of(_bus, struct dummy_bus, base_bus);
  *value = dummy_bus_read_reg(bus, reg);
  return 0;
}

static int dummy_read_u32(struct rtuart_bus * _bus, const int reg, u32 * value)
{
  struct dummy_bus * bus = container_of(_bus, struct dummy_bus, base_bus);
  *value = dummy_bus_read_reg(bus, reg);
  return 0;
}

static int dummy_write_u8(struct rtuart_bus * _bus, const int reg, const u8 value)
{
  struct dummy_bus * bus = container_of(_bus, struct dummy_bus, base_bus);
  dummy_bus_write_reg(bus, reg, value);
  return 0;
}

static int dummy_write_u16(struct rtuart_bus * _bus, const int reg, const u16 value)
{
  struct dummy_bus * bus = container_of(_bus, struct dummy_bus, base_bus);
  dummy_bus_write_reg(bus, reg, value);
  return 0;
}


static int dummy_write_u32(struct rtuart_bus * _bus, const int reg, const u32 value)
{
  struct dummy_bus * bus = container_of(_bus, struct dummy_bus, base_bus);
  dummy_bus_write_reg(bus, reg, value);
  return 0;
}

static int dummy_cleanup(struct rtuart_bus * _bus)
{
  struct dummy_bus * bus = container_of(_bus, struct dummy_bus, base_bus);
  // close(bus->fd);
  printf ("dummy_cleanup()\n");
  free(bus);
  return 0;
}

struct rtuart_bus_ops uart_dummy_bus_ops =
{
  .read_u8   = dummy_read_u8,
  .write_u8  = dummy_write_u8,
  .read_u16  = dummy_read_u16,
  .write_u16 = dummy_write_u16,
  .read_u32  = dummy_read_u32,
  .write_u32 = dummy_write_u32,
  .cleanup   = dummy_cleanup
};

struct rtuart_bus * dummy_bus_create(const char * arg)
{
  struct dummy_bus * bus = 0;

  (void)arg;
  printf ("dummy_bus_create()\n");

  bus = (struct dummy_bus*)malloc(sizeof(struct dummy_bus));
  if (bus)
    {
      bus->base_bus.ops = &uart_dummy_bus_ops;
      dummy_bus_init_internal_registers(bus->register_values);
    }
  return &bus->base_bus;
}

