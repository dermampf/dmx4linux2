// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#include "rdmcommands.hpp"

#include <dmx.hpp>
#include <rdmvalues.hpp>
#include "dmx4linux2interface.hpp"
#include "dmx4linux2.hpp"
#include "constdmxdata.hpp"
#include "rdmapi.hpp"

#include <unistd.h>

RdmInterpreter::RdmInterpreter(Rdm::RdmApi & _rdm)
    : m_rdm(_rdm)
{
}

RdmInterpreter::~RdmInterpreter()
{
}

void RdmInterpreter::foreachFunction(IteratorFunction c)
{
    m_topCommands =
	std::vector<std::string>
	{
	    "mute",
	    "unmute",
	    "discovery",
	    "listdevices",
	    "get",
	    "set",
	    "test"
	};

    using namespace std::placeholders;
    c("mute",       std::bind(&RdmInterpreter::mute,     this, _1, _2));
    c("unmute",     std::bind(&RdmInterpreter::unmute,   this, _1, _2));
    c("discovery",  std::bind(&RdmInterpreter::discovery,this, _1, _2));
    c("listdevices",std::bind(&RdmInterpreter::listdevices,this, _1, _2));
    c("get",        std::bind(&RdmInterpreter::get,      this, _1, _2));
    c("set",        std::bind(&RdmInterpreter::set,      this, _1, _2));
    c("test",       std::bind(&RdmInterpreter::test,     this, _1, _2));
}


bool RdmInterpreter::mute(std::ostream & out, const Arguments & args)
{
    if (args.size() > 1)
	m_rdm.mute(Rdm::Uid(args[1]));
    return true;
}

bool RdmInterpreter::unmute(std::ostream & out, const Arguments & args)
{
    if (args.size() > 1)
	m_rdm.unmute(Rdm::Uid(args[1]));
    return true;
}

bool RdmInterpreter::discovery(std::ostream & out, const Arguments & args)
{
    if (args.size() == 3)
	m_rdm.startDiscovery(Rdm::Uid(args[1]), Rdm::Uid(args[2]));
    else if (args.size() == 2)
	m_rdm.startDiscovery(Rdm::Uid(args[1]), Rdm::Uid(args[1]), 1);
    else
	m_rdm.startDiscovery();
    return true;
}

bool RdmInterpreter::listdevices(std::ostream & out, const Arguments & args)
{
    const std::vector<Rdm::Uid> d = m_rdm.devicesDiscovered();
    for (auto i : d)
    {
	out << std::hex << uint64_t(i)
	    << std::endl;
    }
    return true;
}

bool RdmInterpreter::set(std::ostream & out, const Arguments & args)
{
    if (args.size() > 2)
    {
	if (args[2] == "address")
	{
	    if (args.size() > 3)
		m_rdm.set(Rdm::Uid(args[1]), Rdm::DMX_START_ADDRESS).add(uint16_t(strtol(args[3].c_str(),0,0)));
	}
	else if (args[2] == "identify")
	{
	    if (args.size() > 3)
		m_rdm.set(Rdm::Uid(args[1]), Rdm::IDENTIFY_DEVICE).add(uint8_t(strtol(args[3].c_str(),0,0)));
	}
	else if (args[2] == "factory-uid")
	{
	    if (args.size() > 3)
	      m_rdm.set(Rdm::Uid(args[1]), 0xFEED).add(Rdm::Uid(args[3]));
	}
    }
    return true;
}

bool RdmInterpreter::get(std::ostream & out, const Arguments & args)
{
    if (args.size() > 2)
      {
	if (args[2] == "address")
	  {
	    Rdm::RdmFuture & f = m_rdm.get(Rdm::Uid(args[1]), Rdm::DMX_START_ADDRESS);
	    out << "dmx-address:" << f.at<uint16_t>(0) << std::endl;
	  }
	else if (args[2] == "identify")
	{
	    Rdm::RdmFuture & f = m_rdm.get(Rdm::Uid(args[1]), Rdm::IDENTIFY_DEVICE);
	    out << "identify:" << f.at<uint8_t>(0) << std::endl;
	}
	else if (args[2] == "info")
	{
	    Rdm::RdmFuture & f = m_rdm.get(Rdm::Uid(args[1]), Rdm::DEVICE_INFO);
	    f.wait();
	    out << std::hex;
	    out << "RDM Protocol Version:" << f.at<uint16_t>(0) << std::endl;
	    out << "DeviceModel ID:" << f.at<uint16_t>(2) << std::endl;
	    out << "Product Category:" << f.at<uint16_t>(4) << std::endl;
	    out << "Software Version:" << f.at<uint32_t>(6) << std::endl;
	    out << "DMX512 Footprint:" << f.at<uint16_t>(10) << std::endl;
	    out << "DMX512 Personality:" << f.at<uint16_t>(12) << std::endl;
	    out << "DMX512 Start Address:" << f.at<uint16_t>(14) << std::endl;
	    out << "SubDevice Count:" << f.at<uint16_t>(16) << std::endl;
	    out << "Sensor Count:" << (int)(f.at<uint8_t>(18)) << std::endl;
	}
	else if (args[2] == "swversion")
	{
	    Rdm::RdmFuture & f = m_rdm.get(Rdm::Uid(args[1]), Rdm::SOFTWARE_VERSION_LABEL);
	    const int s = f.size();
	    char str[256] = "";
	    f.at(0, str, s);
	    str[s] = 0;
	    out << "SW-Version[" << s << "]: '" << str << "'" << std::endl;
	}
      }
    return true;
}

bool RdmInterpreter::test(std::ostream & out, const Arguments & args)
{
    if (args.size() > 1)
    {
	const int numberIterations = strtol(args[1].c_str(), 0, 0);
	for (int i = 0; i < numberIterations; ++i)
	{
	  //  printf ("discover\n");
	    m_rdm.startDiscovery();

	    usleep(100*1000);

	   // printf ("list\n");
	    const std::vector<Rdm::Uid> d = m_rdm.devicesDiscovered();
	    for (auto a : d)
	    {
		out << std::hex << uint64_t(a) << " ";
	    }
	    out << std::endl;

	    usleep(100*1000);

	   // printf ("unmute all\n");
	    m_rdm.unmute(Rdm::Uid::broadcast());
	    usleep(100*1000);
	}
    }
    return true;
}
