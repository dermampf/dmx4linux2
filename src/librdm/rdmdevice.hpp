#pragma once
#include <dmx.hpp>
#include <stdint.h>

class RdmDeviceP;
class RdmDevice : public DmxTransceiver
{
private:
  RdmDeviceP & m;
public:
  RdmDevice(const char * name, const uint64_t _myUid);
  ~RdmDevice();

  void name(const char * _name);
  const char * name() const;

  void myUid(const uint64_t _myUid);
  uint64_t myUid() const;
  bool isMe(const uint64_t _myUid);

  void dmx512Address(const int & a);
  int  dmx512Address() const;

  void handle(const IDmxData & f);
private:
  void sendReply(const IDmxData & f);
};
