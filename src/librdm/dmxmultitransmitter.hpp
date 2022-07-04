// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#pragma once
#include "dmx.hpp"
#include <vector>
class DmxMultiTransmitter : public IDmxTransmitter
{
private:
  std::vector<IDmxReceiver*>  m_receivers;
public:
  virtual void registerReceiver(IDmxReceiver & r)
  {
    m_receivers.push_back(&r);
  }
protected:
  void handleTx(const IDmxData & f)
  {
    for (auto i : m_receivers)
      i->handle(f);
  }
};
