// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#ifndef DEFINED_RTUART_BUS_OPS
#define DEFINED_RTUART_BUS_OPS

#include "kernel.h"

/*
 * Read and write registers.
 * @reg is the register number, not the memory location.
 * @value is 32 bit bit only 8 bit may be used, if the uart has 8bit wide registers.
 */
struct rtuart_bus_ops
{
  int (*read_u8)(struct rtuart_bus *, const int reg, u8 * value);
  int (*write_u8)(struct rtuart_bus *, const int reg, const u8 value);
  int (*read_u16)(struct rtuart_bus *, const int reg, u16 * value);
  int (*write_u16)(struct rtuart_bus *, const int reg, const u16 value);
  int (*read_u32)(struct rtuart_bus *, const int reg, u32 * value);
  int (*write_u32)(struct rtuart_bus *, const int reg, const u32 value);
  int (*cleanup)(struct rtuart_bus *);
  // may have read_u64, write_u64 in the future.
};

#endif
