#pragma once
struct dmx512frame;
struct pollfd;

/*
 * This abstracts most of the operating system away.
 * It will get some more methods that use ioctl's
 * to get and set some device specific attributes.
 */
class Dmx4Linux2Device
{
private:
  int m_dmxfd;
public:
  Dmx4Linux2Device();

  /* @name the name of the device.
   * If the environment variable DMXDEVICE is set it is used
   * instead of the supplied name. If no name or 0 is supplied
   * the value of the environment variable DEFAULT_DMXDEVICE
   * and if that is not supplied the default internal name is
   * used, which currently is "/dev/dmx-card0".
   */
  bool open(const char * name = 0);

  // close the device.
  void close();

  // write one frame to the device
  bool write(dmx512frame & dmxframe);

  // read a frame and block, if no one is available.
  bool read(dmx512frame & dmxframe);

  // adds the file handle to a pollfd entry.
  void addToPoll(struct pollfd &);

  operator bool () const;
};
