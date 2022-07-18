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

int main (int argc , char **argv)
{
    const char * card_name = (argc > 1) ? argv[1] : "/dev/dmx-card0";
    int dmxfd = open(card_name, O_RDWR);
    if (dmxfd < 0)
	return 1;


    
    int count = 0;
    while (1)
    {
	struct dmx512frame frame;
	bzero(&frame, sizeof(frame));
	int n = read (dmxfd, &frame, sizeof(frame));
        if (n < 0)
        {
          perror("read");
          continue;
        }
        printf ("[%d]\n", count++);
        printf (" port:%d\n", frame.port);
        printf (" breaksize:%d\n", frame.breaksize);
        printf (" startcode:%d\n", frame.startcode);
        printf (" #slots:%d\n", frame.payload_size);
        printf ("  slots:");
        int i;
        for (i = 0; i < 10 && i < frame.payload_size; ++i)
          printf (" %02X", frame.payload[i]);
        printf ("\n");
    }
    close(dmxfd);
    return 0;
}
