// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#include <linux/dmx512/dmx512frame.h>

#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>

static void prepareRdmDiscoverUniqueBranch(struct dmx512frame *frame,
					   unsigned long long start,
					   unsigned long long end)
{
  const char frameHex[] = "CC 01 24 FF FF FF FF FF FF 4D 41 00 00 00 01 00 00 00 00 00 10 00 01 0C 00 00 00 00 00 00 FF FF FF FF FF FE 0D 90";
  const size_t frameHexLen = strlen(frameHex);
  int dstIdx = 0;
  for (int srcIdx = 0; srcIdx < frameHexLen; srcIdx+=3, dstIdx++)
    {
      frame->data[dstIdx] = (unsigned char)strtoul(&frameHex[srcIdx], NULL, 16);
    }

  unsigned short crc = 0;
  for (int i=0; i < dstIdx-2; i++)
    {
      crc += frame->data[i];
    }
  frame->data[dstIdx-2] = (unsigned char)(crc>>8); // Checksum-High
  frame->data[dstIdx-1] = (unsigned char)crc;      // Checksum-Low

  for (int i = 0; i < dstIdx; i++)
    {
      printf ("%02X ", frame->data[i]);
    }
  printf ("\n");

  frame->startcode = 0xCC;
  frame->payload_size = dstIdx - 1;
}


/*#include <fcntl.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>*/
static int dmx_open(int * portNumber)
{
  char deviceName[PATH_MAX];
  int portNum = -1;
  strcpy(deviceName, getenv("DMX") ?: "/dev/dmx-card0");

  char * p = index(deviceName, ':');
  if(p)
    {
      *(p++) = 0;
      portNum = strtoul(p, 0, 0);
    }
  else
    portNum = 0;

  if (portNumber)
    *portNumber = portNum;
  return open (deviceName, O_RDWR);
}

int main (int argc, char **argv)
{
  int portNumber = -1;
  int fd = dmx_open(&portNumber);
  if ((fd < 0) || (portNumber < 0))
    return 1;

  struct dmx512frame frame;
  frame.port = (portNumber >= 0) ? portNumber : 0;
  frame.breaksize = 0; // default

  do
    {
      usleep(250 * 1000);

      prepareRdmDiscoverUniqueBranch(&frame, 0, 0xFFFFFFFFFFFFLL);
      write (fd, &frame, sizeof(frame));

      const int n = read (fd, &frame, sizeof(frame));
      if (n == sizeof(frame))
	{
	  printf ("port: %d\n", frame.port);
	  printf ("  slotcount: %d\n", frame.payload_size + 1);
	  printf ("  data:");
	  int i;
	  for (i = 0; i < frame.payload_size + 1; ++i)
	    printf (" %02X", frame.data[i]);
	  printf ("\n");
	}

      // usleep(250 * 1000);
    }  while(0);

  close (fd);
  return 0;
}
