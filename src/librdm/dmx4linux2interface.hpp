#pragma once
#include "dmx.hpp"

class Dmx4Linux2Device;
class Dmx4Linux2Interface : public DmxTransceiver
{
private:
  Dmx4Linux2Device & m_device;
public: // IDmxReceiver
  Dmx4Linux2Interface (Dmx4Linux2Device & _device);
  void handle(const IDmxData & f);
  void handleIngressData();
};
