// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 Michael Stickel <michael@cubic.org>
 */
#include <linux/dmx512/dmx512frame.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

/*
 * send out one dmx-frame to all cards
 * If you open /dev/dmx512/card0port0 that frame is output only to that port.
 */

int main (int argc, char **argv)
{
    const char * card_name = (argc > 1) ? argv[1] : "/dev/dmx-card0";
    int dmxfd = open(card_name, O_RDWR);
    if (dmxfd < 0)
	return 1;
    const int count = (argc > 2) ? atoi(argv[2]) : 10;
    const int portno = (argc > 3) ? atoi(argv[3]) : 1;
    const int slotcount = (argc > 4) ? atoi(argv[4]) : 512;

    int i;
    for (i = 0; i < count; ++i)
    {
	struct dmx512frame frame;
	bzero(&frame, sizeof(frame));
	frame.port = portno;
	frame.breaksize = 0; // default
	frame.startcode = 0;
	frame.payload_size = slotcount;
	//memset (frame.payload, 0xff, 512); // all lamps on
        int j;
        for (j = 0; j < 512; ++j)
          frame.payload[j] = (unsigned char)(j+1);
	write (dmxfd, &frame, sizeof(frame));
	printf ("wrote frame @ %s:%d #slots %d\n",
		card_name,
		portno,
		slotcount);
	usleep(33*1000);
    }

    close(dmxfd);

    return 0;
}
