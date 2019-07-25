#pragma once
/*
 * This bus can be used to connect multiple
 * RDM capable slaves to one master.
 * You can register one master to the masterPort
 * and multiple slaves to the slavePort.
 */

#include <dmx.hpp>
#include <vector>

class DmxBusP;
class DmxBus
{
private:
  DmxBusP & p;
public:
    DmxBus();
    ~DmxBus();
    IDmxTransceiver & masterPort();
    IDmxTransceiver & slavePort();
};
