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

#include "dump_dmx512_frame.h"

/*
 * send out one dmx-frame to all cards
 * If you open /dev/dmx512/card0port0 that frame is output only to that port.
 */

static int generate_pattern(unsigned char * data,
			    const int framesize)
{
    int i;
    for (i = 0; i < framesize; ++i)
        data[i] = (unsigned char)i;
    return framesize;
}

int main (int argc, char **argv)
{
    const char * card_name = (argc > 1) ? argv[1] : "/dev/dmx-card0";
    int dmxfd = open(card_name, O_RDWR);
    if (dmxfd < 0)
	return 1;

    const int framesize = (argc > 3) ? atoi(argv[3]) : 513;

    struct dmx512frame frame;
    bzero(&frame, sizeof(frame));
    frame.port = (argc > 2) ? atoi(argv[2]) : 0;
    frame.breaksize = 0;
    frame.payload_size = generate_pattern(frame.data, framesize) - 1;
    write (dmxfd, &frame, sizeof(frame));

    dump_dmx512_frame (-1, &frame);

    close(dmxfd);

    return 0;
}
