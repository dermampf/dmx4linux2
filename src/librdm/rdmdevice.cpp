// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#include <rdmdevice.hpp>

#include <iostream>
#include <iomanip>
#include <assert.h>
#include <string.h>

#include "constdmxdata.hpp"
#include <rdmvalues.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

class RdmDeviceP
{
public:
    RdmDeviceP()
	:  name(0),
	   myUid(0),
	   muted(false)
	{
	}
    const char *   name;
    uint64_t       myUid;
    bool           muted;

    uint16_t deviceModelId;     // Device Model ID
    uint16_t productCategory;   // Product Category
    uint32_t softwareVersion;   // Software Version
    uint16_t dmx512Footprint;   // DMX512 Footprint
    uint16_t dmx512Personality; // DMX512 Personality
    uint16_t dmxAddress;        // DMX512 Start Address
    uint8_t  identifyOn;
};

RdmDevice::RdmDevice(const char * _name, const uint64_t _myUid)
  : m(* new RdmDeviceP)
{
    name(_name);
    myUid(_myUid);


    m.deviceModelId = 0x4711;
    m.productCategory = 0;
    m.softwareVersion = 0x12345678;
    m.dmx512Footprint = 20;
    m.dmx512Personality = 0;
    m.dmxAddress = 0;
    m.identifyOn = false;
}

RdmDevice::~RdmDevice()
{
    delete & m;
}

void RdmDevice::name(const char * _name)
{
    m.name = _name;
}

const char * RdmDevice::name() const
{
    return m.name;
}

void RdmDevice::myUid(const uint64_t _myUid)
{
    m.myUid = _myUid;
}

uint64_t RdmDevice::myUid() const
{
    return m.myUid;
}

bool RdmDevice::isMe(const uint64_t _uid)
{
  return (_uid == Rdm::Uid::broadcast()) || (_uid == myUid());
}

void RdmDevice::dmx512Address(const int & a)
{
    m.dmxAddress = a;
}

int RdmDevice::dmx512Address() const
{
    return m.dmxAddress;
}

void RdmDevice::sendReply(const IDmxData & f)
{
    // std::cout << "RDM-Reply:" << std::endl;
    // dump(f.data(), f.size());
    handleTx(f);
}

void RdmDevice::handle(const IDmxData & f)
{
    Rdm::PacketDecoder rdm(f.data(), f.size());
    unsigned char buffer[256];
    memset(buffer, 0, sizeof(Rdm::Header));

#if 0
    std::cout << "-----------------------------------" << std::endl;
    std::cout << "RDM-Frame-RX:" << std::endl;
    dump(f.data(), f.size());
    std::cerr << std::hex
	      << "isRdm:" << rdm.isRdm() << std::endl
	      << "isRequest:" << rdm.isRequest() << std::endl
	      << "SubDevice:" << rdm.SubDevice() << std::endl
	      << "CommandClass:" << rdm.CommandClass() << std::endl
	      << "ItIsForMe: " << rdm.ItIsForMe(myUid()) << std::endl
	;
#endif
    if (rdm.isRdm())
    {
	if (rdm.isRequest() && rdm.ItIsForMe(myUid()))
	{
	    switch(rdm.CommandClass())
	    {
	    case Rdm::DISCOVERY_COMMAND:
		switch (rdm.Pid())
		{
		case Rdm::DISC_UNIQUE_BRANCH:
		    // std::cerr << "DiscoveryRequest::DISC_UNIQUE_BRANCH" << std::endl;
		    if (!m.muted)
		    {
			uint64_t from;
			uint64_t to;
			if (!rdm.PdAt(0, from, 6) || !rdm.PdAt(6, to, 6))
			    return;
#if 0
			std::cerr << "  search = {"
				  << std::hex << from << ".." << to
				  << "} my:" << myUid()
				  << std::endl;
#endif
			if ((myUid() >= from) && (myUid() <= to))
			{
			    // std::cerr << "    -> do reply " << std::endl;
			    const int size = Rdm::PacketEncoder(buffer,sizeof(buffer))
				.Source(myUid())
				.Destination(rdm.Source())
				.CommandClass(Rdm::DISCOVERY_COMMAND_RESPONSE)
				.Pid(Rdm::DISC_UNIQUE_BRANCH)
				.ResponseType(Rdm::RESPONSE_TYPE_ACK)
				.finalize().size();
			    sendReply(ConstDmxData((const void *)&buffer, size));
			}
		    }
#if 0
		    else
			std::cerr << "  " << myUid() << " is muted" << std::endl;
#endif
		    break;

		case Rdm::DISC_MUTE:
		    // std::cerr << "DiscoveryRequest::DISC_MUTE" << std::endl;
		    m.muted = true;
		    if (rdm.Destination() != Rdm::Uid::broadcast())
		    {
			const int size = Rdm::PacketEncoder(buffer,sizeof(buffer))
			    .Source(myUid())
			    .Destination(rdm.Source())
			    .CommandClass(Rdm::DISCOVERY_COMMAND_RESPONSE)
			    .Pid(Rdm::DISC_MUTE)
			    .ResponseType(Rdm::RESPONSE_TYPE_ACK)
			    .finalize().size();
			sendReply(ConstDmxData((const void *)&buffer, size));
		    }
		    break;

		case Rdm::DISC_UN_MUTE:
		    // std::cerr << "DiscoveryRequest::DISC_UN_MUTE" << std::endl;
		    m.muted = false;
		    if (rdm.Destination() != Rdm::Uid::broadcast())
		    {
			const int size = Rdm::PacketEncoder(buffer,sizeof(buffer))
			    .Source(myUid())
			    .Destination(rdm.Source())
			    .CommandClass(Rdm::DISCOVERY_COMMAND_RESPONSE)
			    .Pid(Rdm::DISC_UN_MUTE)
			    .ResponseType(Rdm::RESPONSE_TYPE_ACK)
			    .finalize().size();
			sendReply(ConstDmxData((const void *)&buffer, size));
		    }
		    break;

		case Rdm::PROXIED_DEVICES:
		    std::cerr << "DiscoveryRequest::PROXIED_DEVICES" << std::endl;
		    break;
		case Rdm::PROXIED_DEVICE_COUNT:
		    std::cerr << "DiscoveryRequest::PROXIED_DEVICE_COUNT" << std::endl;
		    break;
		case Rdm::COMMS_STATUS:
		    std::cerr << "DiscoveryRequest::COMMS_STATUS" << std::endl;
		    break;
		default:
		    std::cerr << "DiscoveryRequest::" << rdm.CommandClass() << std::endl;
		    break;
		}
		break;

	    case Rdm::GET_COMMAND:
	      std::cout << "Rdm::GET_COMMAND" << std::endl;
	      // if (rdm.Destination() != myUid()) return;
		{
		    Rdm::PacketEncoder e(buffer,sizeof(buffer));
		    e   .Source(myUid())
			.Destination(rdm.Source())
			.CommandClass(Rdm::GET_COMMAND_RESPONSE)
			.Pid(Rdm::DEVICE_INFO)
			.ResponseType(Rdm::RESPONSE_TYPE_ACK);

		    switch (rdm.Pid())
		    {
		    case Rdm::DEVICE_INFO:
			std::cerr << "Get DEVICE_INFO" << std::endl;
			e   .addPd(uint16_t(0x100)) // RDM Protocol Version
			    .addPd(uint16_t(m.deviceModelId)) // Device Model ID
			    .addPd(uint16_t(m.productCategory)) // Product Category
			    .addPd(uint32_t(m.softwareVersion)) // Software Version
			    .addPd(uint16_t(m.dmx512Footprint)) // DMX512 Footprint
			    .addPd(uint16_t(m.dmx512Personality)) // DMX512 Personality
			    .addPd(uint16_t(m.dmxAddress)) // DMX512 Start Address
			    .addPd(uint16_t(0)) // Sub-Device Count -- this implementation has no sub-devices
			    .addPd(uint8_t(0));  // Sensor count -- this implementation has no sensors
			break;

		    case Rdm::SOFTWARE_VERSION_LABEL:
			std::cerr << "Get SOFTWARE_VERSION_LABEL" << std::endl;
			e.addPd("V1.0.0");
			break;

		    case Rdm::DMX_START_ADDRESS:
			std::cerr << "Get DMX_START_ADDRESS" << std::endl;
			e.addPd(uint16_t(m.dmxAddress));
			break;

		    case Rdm::IDENTIFY_DEVICE:
			std::cerr << "Get IDENTIFY_DEVICE" << std::endl;
			e.addPd(uint8_t(m.identifyOn));
			break;

		    default:
			e.ResponseType(Rdm::RESPONSE_TYPE_UNKNOWN_PID);
			break;
		    };
		    sendReply(ConstDmxData((const void *)&buffer, e.finalize().size()));
		}
		break;
		

	    case Rdm::SET_COMMAND:
	      // if (rdm.Destination() != myUid()) return;
	      std::cout << "Rdm::SET_COMMAND:" << std::hex << rdm.Pid() << std::endl;
		{
		    Rdm::PacketEncoder e(buffer,sizeof(buffer));
		    e   .Source(myUid())
			.Destination(rdm.Source())
			.CommandClass(Rdm::GET_COMMAND_RESPONSE)
			.Pid(Rdm::DEVICE_INFO)
			.ResponseType(Rdm::RESPONSE_TYPE_ACK);

		    switch (rdm.Pid())
		    {
		    case Rdm::DMX_START_ADDRESS:
			std::cerr << "Set DMX_START_ADDRESS" << std::endl;
			if (rdm.PdAt(0, m.dmxAddress))
			  std::cout << "new StartAddress = " << m.dmxAddress << std::endl;
			break;

		    case Rdm::IDENTIFY_DEVICE:
			std::cerr << "Set IDENTIFY_DEVICE" << std::endl;
			if (rdm.PdAt(0, m.identifyOn))
			    std::cout << "Identify:" << m.identifyOn << std::endl;
			break;

		    case 0xFEED: // set UID
			std::cerr << "Set 0xFEED" << std::endl;
		        {
			    uint64_t newUid = 0;
			    if (rdm.PdAt(0, newUid, 6))
			    {
				myUid(newUid);
				std::cout << "newUID:" << myUid() << std::endl;
			    }
			}
			break;

		    default:
			e.ResponseType(Rdm::RESPONSE_TYPE_UNKNOWN_PID);
			break;
		    };
		    sendReply(ConstDmxData((const void *)&buffer, e.finalize().size()));
		}
		break;
	    }
	}
    }
    else if (rdm.isDmx())
    {
    }
}
