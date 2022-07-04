// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#pragma once

#include <dmx.hpp>

class BufferDmxData : public IDmxData
{
  unsigned char m_buffer[513];
  int           m_size;
public:
  BufferDmxData() : m_size(0)               { }
  int          size() const                 { return m_size; }
  const void * data() const                 { return m_buffer; }
  void size(const int newSize)              { m_size = newSize; }
  unsigned char & operator [] (const int i) { return m_buffer[i]; }
  unsigned char * data()                    { return m_buffer; }
};
