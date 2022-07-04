// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
/*
 * This file contains all known values from the RDM specification.
 */
#pragma once

#define STRUCT_PACKED __attribute__((packed))

namespace Rdm
{
  enum {
    SC_RDM = 0xCC,
    STATUS_ERROR = 0xFF,
    SC_SUB_MESSAGE = 0x01,
  };

  enum RdmFieldIndex
    {
      RDMFIELD_STARTCODE=0,  // 0
      RDMFIELD_SUBSTARTCODE, // 1
      RDMFIELD_MESSAGELENGTH,// 2

      RDMFIELD_DEST_UID0,    // 3
      RDMFIELD_DEST_UID1,    // 4
      RDMFIELD_DEST_UID2,    // 5
      RDMFIELD_DEST_UID3,    // 6
      RDMFIELD_DEST_UID4,    // 7
      RDMFIELD_DEST_UID5,    // 8

      RDMFIELD_SRC_UID0,     // 9
      RDMFIELD_SRC_UID1,     // 10
      RDMFIELD_SRC_UID2,     // 11
      RDMFIELD_SRC_UID3,     // 12
      RDMFIELD_SRC_UID4,     // 13
      RDMFIELD_SRC_UID5,     // 14

      RDMFIELD_TRANSACTION_NUMBER,  // 15
      RDMFIELD_PORTID_RESPONSETYPE,  // 16
      RDMFIELD_MESSAGE_COUNT,  // 17

      RDMFIELD_SUBDEVICE0,  // 18
      RDMFIELD_SUBDEVICE1,  // 19

      RDMFIELD_COMMANDCLASS, // 20 - CC

      RDMFIELD_PARAMETER_ID0,  // 21
      RDMFIELD_PARAMETER_ID1,  // 22

      RDMFIELD_PARAMETER_DATA_LENGTH, // 23 - PDL

      RDMFIELD_PARAMETER_DATA, // 24 - and the following
    };

  enum RdmCommandClass
    {
      DISCOVERY_COMMAND = 0x10,
      DISCOVERY_COMMAND_RESPONSE = 0x11,
      GET_COMMAND = 0x20,
      GET_COMMAND_RESPONSE = 0x21,
      SET_COMMAND = 0x30,
      SET_COMMAND_RESPONSE = 0x31
    };

  enum RdmResponseType
    {
      //-- Table A-2: Response Type Defines
      //--- RDM Response Type (Slot 16)  Value       Comment
      RESPONSE_TYPE_ACK            = 0x00,
      RESPONSE_TYPE_ACK_TIMER      = 0x01,
      RESPONSE_TYPE_NACK_REASON    = 0x02,       // See Table A-17
      RESPONSE_TYPE_ACK_OVERFLOW   = 0x03,       // Additional Response Data available beyond single response length.

      RESPONSE_TYPE_UNKNOWN_PID = 0xfe, // This is used internally to indicate, that the pid is unknown
      RESPONSE_TYPE_NOREPLY = 0xff // This is used internally to indicate, that no response shall be send
    };

  enum RdmParameterIds
    {
      //-- RDM Parameter ID's (Slot 21-22) Value  Comment
      //--- Category - Network Management

      DISC_UNIQUE_BRANCH             = 0x0001,
      DISC_MUTE                      = 0x0002,
      DISC_UN_MUTE                   = 0x0003,
      PROXIED_DEVICES                = 0x0010,
      PROXIED_DEVICE_COUNT           = 0x0011,
      COMMS_STATUS                   = 0x0015,

      //--- Category - Status Collection
      QUEUED_MESSAGE                 = 0x0020, // See Table A-4
      STATUS_MESSAGES                = 0x0030, // See Table A-4
      STATUS_ID_DESCRIPTION          = 0x0031,
      CLEAR_STATUS_ID                = 0x0032,
      SUB_DEVICE_STATUS_REPORT_THRESHOLD = 0x0033, // See Table A-4


      //--- Category - RDM Information
      SUPPORTED_PARAMETERS           = 0x0050,  // * Support required only if supporting Parameters beyond the minimum required set.
      PARAMETER_DESCRIPTION          = 0x0051, // - Support required for Manufacturer-Specific PIDs exposed in SUPPORTED_PARAMETERS message.

      //--- Category - Product Information
      DEVICE_INFO                    = 0x0060,
      PRODUCT_DETAIL_ID_LIST         = 0x0070,
      DEVICE_MODEL_DESCRIPTION       = 0x0080,
      MANUFACTURER_LABEL             = 0x0081,
      DEVICE_LABEL                   = 0x0082,
      FACTORY_DEFAULTS               = 0x0090,
      LANGUAGE_CAPABILITIES          = 0x00A0,
      LANGUAGE                       = 0x00B0,
      SOFTWARE_VERSION_LABEL         = 0x00C0,
      BOOT_SOFTWARE_VERSION_ID       = 0x00C1,
      BOOT_SOFTWARE_VERSION_LABEL    = 0x00C2,

      //--- Category - DMX512 Setup
      DMX_FOOTPRINT                  = 0x00D0,
      DMX_PERSONALITY                = 0x00E0,
      DMX_PERSONALITY_DESCRIPTION    = 0x00E1,
      DMX_START_ADDRESS              = 0x00F0, // * Support required if device uses a DMX512 Slot.
      SLOT_INFO                      = 0x0120,
      SLOT_DESCRIPTION               = 0x0121,
      DEFAULT_SLOT_VALUE             = 0x0122,

      //--- Category - Sensors              0x02xx
      SENSOR_DEFINITION              = 0x0200,
      SENSOR_VALUE                   = 0x0201,
      RECORD_SENSORS                 = 0x0202,

      //-- RDM Parameter ID's (Slot 21-22) Value   Comment

      //--- Category - Dimmer Settings      0x03xx  Future

      //--- Category - Power/Lamp Settings  0x04xx
      DEVICE_HOURS                   = 0x0400,
      LAMP_HOURS                     = 0x0401,
      LAMP_STRIKES                   = 0x0402,
      LAMP_STATE                     = 0x0403,  // See Table A-8
      LAMP_ON_MODE                   = 0x0404,  // See Table A-9
      DEVICE_POWER_CYCLES            = 0x0405,
      //--- Category - Display Settings     0x05xx
      DISPLAY_INVERT                 = 0x0500,
      DISPLAY_LEVEL                  = 0x0501,

      //--- Category - Configuration        0x06xx
      PAN_INVERT                     = 0x0600,
      TILT_INVERT                    = 0x0601,
      PAN_TILT_SWAP                  = 0x0602,
      REAL_TIME_CLOCK                = 0x0603,

      //--- Category - Control              0x10xx
      IDENTIFY_DEVICE                = 0x1000,
      RESET_DEVICE                   = 0x1001,
      POWER_STATE                    = 0x1010,  // See Table A-11
      PERFORM_SELFTEST               = 0x1020,  // See Table A-10
      SELF_TEST_DESCRIPTION          = 0x1021,
      CAPTURE_PRESET                 = 0x1030,
      PRESET_PLAYBACK                = 0x1031  // See Table A-7

      // ESTA Reserved Future RDM Development   0x7FE0-0x7FFF

      // Manufacturer-Specific PIDs             0x8000-0xFFDF

      // ESTA Reserved Future RDM Development   0xFFE0-0xFFFF
    };
};


#include <arpa/inet.h>
#include <string.h>

#include <string>

namespace Rdm
{

    // can directly be embedded into the buffer on the network.
    class Uid
    {
    private:
	struct
	{
	    uint16_t m_manufacturer; //   2 bytes
	    uint32_t m_device;       // + 4 bytes
	} STRUCT_PACKED;         // = 6 bytes
    public:
	Uid ()
	    : m_manufacturer(0),
	      m_device(0)
	    {
	    }
	Uid (const uint64_t _uid)
	    {
		manufacturer(uint16_t(_uid>>32));
		device(uint32_t(_uid));
	    }
	Uid (const char * _uidstr)
	    {
		const uint64_t uid = strtoull(_uidstr, 0, 16);
		manufacturer(uint16_t(uid>>32));
		device(uint32_t(uid));
	    }
	Uid (const std::string & _uidstr)
	    {
		const uint64_t uid = strtoull(_uidstr.c_str(), 0, 16);
		manufacturer(uint16_t(uid>>32));
		device(uint32_t(uid));
	    }
	operator uint64_t () const
	    {
		return (((uint64_t)manufacturer())<<32) + device();
	    }
	uint16_t manufacturer() const
	    {
		return ntohs(m_manufacturer);
	    }
	uint32_t device() const
	    {
		return ntohl(m_device);
	    }
	void manufacturer(const uint16_t m) 
	    {
		m_manufacturer = htons(m);
	    }
	void device(const uint32_t d)
	    {
		m_device = htonl(d);
	    }

	bool operator == (const uint64_t & other) const
	    {
		return uint64_t(*this) == other;
	    }

	bool operator == (const Uid & other) const
	    {
		return
		    m_manufacturer == other.m_manufacturer &&
		    m_device == other.m_device;
	    }

	static Uid broadcast()
	    {
		return Uid(0xFFFFFFFFFFFFL);
	    }
	static Uid any()
	    {
		return Uid(uint64_t(0L));
	    }
    } STRUCT_PACKED;


  /*
  struct Uid
  {
    operator uint64_t () const { return (uint64_t(manufacturer)<<32) | device; }
  } STRUCT_PACKED;
  */
  struct Header
  {
    uint8_t     startCode;        // 0        SC_RDM
    uint8_t     subStartCode;     // 1        SC_SUB_MESSAGE
    uint8_t     msgLength;        // 2        Range 24 - 255
    Uid         dstUid;         // 3-8      Destination UID
    Uid         srcUid;         // 9-14     Source UID (sender)
    uint8_t     TN;               // 15       Transaction number
    uint8_t     portId;           // 16       Port ID / Response type
    uint8_t     msgCount;         // 17
    uint16_t    subDevice;        // 18,19    0=root, 0xffff=all
    uint8_t     CC;               // 20       GET_COMMAND
    uint16_t    PID;              // 21,22    Parameter ID
    uint8_t     PDL;              // 23       Parameter Data length 1-231
    // Parameter data of up to 231 octetes and checksum of two octetes follow.
    // uint8_t     PD[RDM_PD_MAXLEN];    // Parameter Data ... variable length
  } STRUCT_PACKED;

  struct Packet
  {
    struct Header header;
    uint8_t       data[256-sizeof(Header)];
  } STRUCT_PACKED;



  static unsigned short calculateChecksum(const void *buffer, const unsigned int size)
  {
    unsigned short sum = 0;
    for (int i = 0; i < size; ++i)
      sum += ((unsigned char *)buffer)[i];
    return sum;
  }


  static void memcpy_reverse(void *_dest, const void * _src, const size_t size)
  {
    unsigned char * dest = (unsigned char *)_dest;
    unsigned char * src  = ((unsigned char *)_src) + size - 1;
    for (int i = 0; i < size; ++i)
      *(dest++) = *(src--);
  }
  
  class PacketDecoder
  {
    const Packet  &    m_pkt;
    const unsigned int m_size;

  public:
    PacketDecoder (const void * _data, const unsigned int _size)
      : m_pkt(*((const Packet *)_data)),
	m_size(_size)
    {
    }

    bool isRdm() const
    {
      return (StartCode() == SC_RDM)
	&& (SubStartCode() == SC_SUB_MESSAGE);
    }

    bool isDmx() const
    {
      return StartCode() == 0;
    }

    bool isRequest() const
    {
      return
	((CommandClass()&0xf0) >= Rdm::DISCOVERY_COMMAND) &&
	((CommandClass()&0xf0) <= Rdm::SET_COMMAND) &&
	((CommandClass() & 0xF)==0) ;
    }

    bool ItIsForMe(const uint64_t myUid) const
    {
      return (Destination() == myUid) || (Destination() == 0xFFFFFFFFFFFFL);
    }

    int StartCode() const
    {
      return m_pkt.header.startCode;
    }
    int SubStartCode() const
    {
      return m_pkt.header.subStartCode;
    }
    int CommandClass() const
    {
      return m_pkt.header.CC;
    }

    int SubDevice() const
    {
      return ntohs(m_pkt.header.subDevice);
    }

    int Pid() const
    {
      return ntohs(m_pkt.header.PID);
    }

    int Pdl() const
    {
      return m_pkt.header.PDL;
    }

    uint64_t Source() const
    {
	return uint64_t(m_pkt.header.srcUid);
    }
    uint64_t Destination() const
    {
	return uint64_t(m_pkt.header.dstUid);
    }

    template <typename T>
    bool PdAt(const int pos, T & v, const int size) const
    {
      if ((pos+size) > Pdl())
	return false;
      memset(&v, 0, sizeof(T)); // TODO: optimize this
      memcpy_reverse((unsigned char *)(&v), &m_pkt.data[pos], size);
      return true;
    }

    template <typename T>
    bool PdAt(const int pos, T & v) const
    {
	return PdAt(pos, v, sizeof(T));
    }

    bool PdAt(const int pos, char * v, const int size) const
    {
      if ((pos+size) > Pdl())
	return false;
      memcpy(v, &m_pkt.data[pos], size);
      return true;
    }

  };

  // const int size = PacketEncoder(data,size).Source(x).Destination(x)
  //                  .CommandClass(x).Pid(x).addPd().finalize().size();
  class PacketEncoder
  {
    Packet  &    m_pkt;
    const unsigned int m_size;

  public:
    PacketEncoder (void * _data, const unsigned int _size)
      : m_pkt(*((Packet *)_data)),
	m_size(_size)
    {
      m_pkt.header.startCode = SC_RDM;
      m_pkt.header.subStartCode = SC_SUB_MESSAGE;
      m_pkt.header.PDL = 0; //- will be increased by addPd(...)
    }

    static bool machineIsBigEndian()
    {
      return ntohl(0x12345678) == 0x12345678;
    }

    PacketEncoder & finalize()
    {
      //-- fixup size field, update checksum and send frame.
      m_pkt.header.msgLength = sizeof(m_pkt.header) + m_pkt.header.PDL;

      //-- calculate checksum
      const unsigned short c = Rdm::calculateChecksum((const void *)&m_pkt,
						      m_pkt.header.msgLength);
      m_pkt.data[m_pkt.header.PDL] = (uint8_t)(c>>8);
      m_pkt.data[m_pkt.header.PDL+1] = (uint8_t)c;
      return *this;
    }

    unsigned int size() const
    {
      return m_pkt.header.msgLength + 2;
    }

    
    PacketEncoder & Source(const unsigned long long src)
    {
	m_pkt.header.srcUid.manufacturer(uint16_t(src>>32));
	m_pkt.header.srcUid.device(uint32_t(src));
	return *this;
    }

    PacketEncoder & Destination(const unsigned long long src)
    {
	m_pkt.header.dstUid.manufacturer(uint16_t(src>>32));
	m_pkt.header.dstUid.device(uint32_t(src));
	return *this;
    }

    PacketEncoder & TransactionNumber(const int tn)
    {
      m_pkt.header.TN = tn;
      return *this;
    }

    PacketEncoder & PortId(const int id)
    {
      m_pkt.header.portId = id;
      return *this;
    }

    PacketEncoder & ResponseType(const int t)
    {
      m_pkt.header.portId = t;
      return *this;
    }

    PacketEncoder & MessageCount(const int c)
    {
      m_pkt.header.msgCount = c;
      return *this;
    }
    
    PacketEncoder & SubDevice(const int sd)
    {
      m_pkt.header.subDevice = htons(sd);
      return *this;
    }

    PacketEncoder & CommandClass(const int cc)
    {
      m_pkt.header.CC = cc;
      return *this;
    }

    PacketEncoder &  Pid(const int pid)
    {
      m_pkt.header.PID = htons(pid);
      return *this;
    }

    PacketEncoder &  addPd(const uint8_t v)
    {
      unsigned char * data = &m_pkt.data[m_pkt.header.PDL];
      *data = v;
      m_pkt.header.PDL += 1;
      return *this;
    }

    template <typename T>
    PacketEncoder &  addPd(const T v)
    {
      unsigned char * data = &m_pkt.data[m_pkt.header.PDL];

      if (machineIsBigEndian())
	memcpy(data, &v, sizeof(v));
      else
	memcpy_reverse(data, &v, sizeof(v));
      m_pkt.header.PDL += sizeof(v);
      return *this;
    }
#if 0
    template <typename T>
    PacketEncoder &  addPd(const T & v)
    {
      unsigned char * data = &m_pkt.data[m_pkt.header.PDL];

      if (machineIsBigEndian())
	memcpy(data, &v, sizeof(v));
      else
	memcpy_reverse(data, &v, sizeof(v));
      m_pkt.header.PDL += sizeof(v);
      return *this;
    }
#endif
    template <typename T>
    PacketEncoder &  addPd(const T & v, const unsigned int _size)
    {
      unsigned char * data = &m_pkt.data[m_pkt.header.PDL];
      const unsigned int size = (_size < sizeof(v)) ? _size : sizeof(v);
      if (machineIsBigEndian())
	memcpy(data, &v, size);
      else
	memcpy_reverse(data, &v, size);
      m_pkt.header.PDL += size;
      return *this;
    }

    template <typename T>
    PacketEncoder &  addPd(const T * v, const unsigned int _size)
    {
	for (int i = 0; i < _size; ++i)
	    addPd(v[i]);
	return *this;
    }

    PacketEncoder &  addPd(const char * str)
    {
      unsigned char * data = &m_pkt.data[m_pkt.header.PDL];
      const unsigned int size = strlen(str);
      memcpy(data, str, size);
      m_pkt.header.PDL += size;
      return *this;
    }

    PacketEncoder & addPd(const Uid & v)
    {
      return addPd(v, 6);
    }


  };

};
