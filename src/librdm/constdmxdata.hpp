// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#pragma once

#include <dmx.hpp>

class ConstDmxData : public IDmxData
{
private:
  const void * m_data;
  const int    m_size;
public:
  ConstDmxData(const void * _data, const int _size)
    : m_data(_data), m_size(_size) { }
  int          size() const { return m_size; }
  const void * data() const { return m_data; }
};
