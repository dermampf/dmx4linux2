// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main (int argc, char ** argv)
{
  if (argc <= 2)
    return 1;

  int fd = open(argv[1], O_RDWR);
  if (fd < 0)
    {
      perror("open uio\n");
      return 1;
    }

  const unsigned long size = 0x10000;
  void * mem = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mem == MAP_FAILED)
    {
      perror("mmap failed");
      printf("  size:0x%08lX\n", size);
      return 1;
    }

  *((unsigned long *)mem) = strtoul(argv[2], 0, 0);

  close(fd);
  return 0;
}

