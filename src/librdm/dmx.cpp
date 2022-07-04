// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#include <dmx.hpp>
#include <iostream>
#include <iomanip>

void dump(const void * _data, const unsigned int size)
{
  std::cout << std::dec;
  std::cout << "   size: " << size << std::endl;
  std::cout << "   data:";
  const unsigned char * data = (const unsigned char *)_data;
  for (int i = 0; i < size; ++i)
    std::cout << " " << std::hex << std::setfill('0') << std::setw(2) << int(data[i]);
  std::cout << std::endl;
}

void dump(const IDmxData & f)
{
  dump(f.data(), f.size());
}
