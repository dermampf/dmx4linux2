// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#include <dmxbus.hpp>
#include <dmx.hpp>
#include <rdmvalues.hpp>
#include <constdmxdata.hpp>
#include <vector>
#include <iostream> // for cerr logging
#include <unistd.h>

class DmxBusP
{
    // The master port is used to connect a master, like a console.
    class MasterPort : public IDmxTransceiver
    {
	DmxBusP & m_bus;
    public:
	MasterPort(DmxBusP & bus) : m_bus(bus) { }
	void registerReceiver(IDmxReceiver & r)
	    {
		m_bus.registerMasterReceiver(r);
	    }
	void handle(const IDmxData & f)
	    {
		m_bus.handleMaster(f);
	    }
    };

    // The slave port is used to connect multiple slaves, like fixtures.
    class SlavePort : public IDmxTransceiver
    {
	DmxBusP & m_bus;
    public:
	SlavePort(DmxBusP & bus) : m_bus(bus) { }
	void registerReceiver(IDmxReceiver & r)
	    {
		m_bus.registerSlaveReceiver(r);
	    }
	void handle(const IDmxData & f)
	    {
		m_bus.handleSlave(f);
	    }
    };

public:
    DmxBusP(DmxBus & _bus)
	: m_masterPort(*this),
	  m_slavePort(*this),
	  m_master(0),
	  m_masterRequestActive(false)
	{
	}

    MasterPort         m_masterPort;
    SlavePort          m_slavePort;
    IDmxReceiver *              m_master;
    std::vector<IDmxReceiver*>  m_slaves;
    bool                        m_masterRequestActive;
    std::vector<std::vector<unsigned char>> m_rxqueue;

public:
    void registerMasterReceiver(IDmxReceiver & r);
    void registerSlaveReceiver(IDmxReceiver & r);
    
    void handleMaster(const IDmxData & f);
    void handleSlave(const IDmxData & f);
};

void DmxBusP::registerMasterReceiver(IDmxReceiver & r)
{
    m_master = &r;
}

void DmxBusP::registerSlaveReceiver(IDmxReceiver & r)
{
    m_slaves.push_back(&r);
}

void DmxBusP::handleMaster(const IDmxData & f)
{
    m_masterRequestActive = true;
    for (auto i : m_slaves)
	i->handle(f);
    m_masterRequestActive = false;

    usleep(3*1000); // 2.7ms reply time for rdm frames.

    // condense all frames received from slaves into one.
    // this might involve some more intelligence.
    // We need to merge multiple discovery replies into a collision frame.
    
    std::cerr << "replies:" << m_rxqueue.size() << std::endl;
    if (m_rxqueue.size() == 1)
    {
	m_master->handle(ConstDmxData(m_rxqueue.front().data(),
				      m_rxqueue.front().size()));
	m_rxqueue.clear();
    }
    else
    {
	//-- check if the frame send is an rdm recovery request.
	Rdm::PacketDecoder rdm(f.data(), f.size());
	if (rdm.isRdm() && rdm.isRequest() &&
	    (rdm.CommandClass() == Rdm::DISCOVERY_COMMAND) &&
	    (rdm.Pid() == Rdm::DISC_UNIQUE_BRANCH))
	{
	    //-- Send Discovery Reply Collision if more than one replied.
	    unsigned char buffer[256];
	    memset(buffer, 0, sizeof(Rdm::Header));
	    const int size = Rdm::PacketEncoder(buffer,sizeof(buffer))
		.Source((m_rxqueue.size() > 1) ? 0xFFFFFFFFFFFFL : 0) // 0xF..F on collision and 0 on timeout.
		.Destination(rdm.Source())
		.CommandClass(Rdm::DISCOVERY_COMMAND_RESPONSE)
		.Pid(Rdm::DISC_UNIQUE_BRANCH)
		.finalize().size();
	    m_master->handle(ConstDmxData((const void *)&buffer, size));
	}
	else if (m_rxqueue.size() > 0)
	{
	    std::cout << "======= Sending frame back ======="
		      << std::endl;
	    m_master->handle(ConstDmxData(m_rxqueue.front().data(),
					  m_rxqueue.front().size()));
	}
	m_rxqueue.clear();
    }
}

void DmxBusP::handleSlave(const IDmxData & f)
{
    if (m_masterRequestActive)
    {
	std::vector<unsigned char> v((unsigned char *)f.data(),
				     ((unsigned char *)f.data()) + f.size());
	m_rxqueue.push_back(v);
    }
}


DmxBus::DmxBus()
    : p(*new DmxBusP(*this))
{
}

DmxBus::~DmxBus()
{
    delete & p;
}

IDmxTransceiver & DmxBus::masterPort()
{
    return p.m_masterPort;
}

IDmxTransceiver & DmxBus::slavePort()
{
    return p.m_slavePort;
}
