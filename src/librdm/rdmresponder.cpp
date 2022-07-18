// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#include <iostream>
#include <dmx.hpp>
#include "dmx4linux2interface.hpp"
#include "dmx4linux2.hpp"
#include "rdmdevice.hpp"
#include "dmxbus.hpp"

#include <vector>

int main (int argc, char **argv)
{
#if 1
    DmxBus dmxbus;
    std::vector<RdmDevice*> devices;
    devices.push_back(new RdmDevice("1", 0x123400000815L));
    devices.push_back(new RdmDevice("2", 0x123400040000L));
    devices.push_back(new RdmDevice("3", 0x123400100000L));
    for (auto i : devices)
    {
	i->registerReceiver(dmxbus.slavePort());
	dmxbus.slavePort().registerReceiver(*i);
    }

    Dmx4Linux2Device    dmx4linux2;
    Dmx4Linux2Interface dmx (dmx4linux2);

    if (!dmx4linux2.open("/dev/dmx-card0"))
    {
	perror("open dmx device");
	return 1;
    }

    dmxbus.masterPort().registerReceiver(dmx);
    dmx.registerReceiver(dmxbus.masterPort());
    while (1)
	dmx.handleIngressData();
#else
    RdmDevice device ("3", 0x123400000002L);
    Dmx4Linux2Device    dmx4linux2;
    Dmx4Linux2Interface dmx (dmx4linux2);

    if (!dmx4linux2.open("/dev/dmx-card0"))
    {
	perror("open dmx device");
	return 1;
    }
    device.registerReceiver(dmx);
    dmx.registerReceiver(device);
    while (1)
	dmx.handleIngressData();
#endif
    return 0;
}
