// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#include "dmx4linux2.hpp"

#include <fcntl.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h> // perror

//#include <dmx.hpp>
#include <unistd.h>
#include <linux/dmx512/dmx512frame.h>

Dmx4Linux2Device::Dmx4Linux2Device()
  : m_dmxfd(-1)
{
}

bool Dmx4Linux2Device::open(const char * name)
{
  close();
  if (getenv("DMXDEVICE"))
    name = getenv("DMXDEVICE");
  if (name == 0)
    name = getenv("DEFAULT_DMXDEVICE");
  if (name == 0)
    name = "/dev/dmx-card0";
  m_dmxfd = ::open(name, O_RDWR | O_NONBLOCK);
  return m_dmxfd >= 0;
}

void Dmx4Linux2Device::close()
{
  ::close(m_dmxfd);
}

Dmx4Linux2Device::operator bool () const
{
  return m_dmxfd >= 0;
}

bool Dmx4Linux2Device::write(dmx512frame & dmxframe)
{
  const int ret = ::write(m_dmxfd, &dmxframe, sizeof(dmxframe));
  if (ret < 0)
    {
      perror("Dmx4Linux2Device::write");
    }
  return ret == sizeof(dmxframe);
}

bool Dmx4Linux2Device::read(dmx512frame & dmxframe)
{
  const int n = ::read(m_dmxfd, &dmxframe, sizeof(dmxframe));
  if (n < 0)
    {
      perror("Dmx4Linux2Device::read");
    }
  return n == sizeof(dmxframe);
}

// adds the file handle to a pollfd entry.
void Dmx4Linux2Device::addToPoll(struct pollfd & fds)
{
  fds.fd = m_dmxfd;
  fds.events = POLLIN;
  fds.revents = 0;
}
