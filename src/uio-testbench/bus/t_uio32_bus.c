// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#include "uio32_bus.h"
#include "rtuart_bus.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main (int argc, char **argv)
{
  int fd;

  if (argc <= 1)
    return 1;

  if ((fd = open(argv[1], O_RDWR)) == -1)
    perror("open uio\n");

  struct rtuart_bus * bus = uio32_create(fd, /*offset:*/0, /*size:*/0x10000 /*0x4096*/);
  if (!bus)
    return 1;

  if (argc > 2)
    {
      const int reg = strtol(argv[2], 0, 0);

      if (argc > 3)
	{
	  const int value = strtol(argv[3], 0, 0);
	  // write
	  if (rtuart_bus_write_u32(bus, reg, value))
	    perror("bus_write_u32");
	}
      else
	{
	  u32 value = 0;
	  if (rtuart_bus_read_u32(bus, reg, &value))
	    perror("bus_read_u32");
	  else
	    printf ("%08lX\n", value);
	}
    }

  return 0;
}
