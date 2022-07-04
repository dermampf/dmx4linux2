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
#include <poll.h>

int main (int argc , char **argv)
{
    const char * card_name = (argc > 1) ? argv[1] : "/dev/dmx-card0";
    int dmxfd = open(card_name, O_RDWR | O_NONBLOCK);
    if (dmxfd < 0)
	return 1;
    int timeout_periods = 0;
    int count = 0;
    while (1)
    {
	struct pollfd fds[1];
	memset (fds, 0, sizeof (fds));
	fds[0].fd = dmxfd;
	fds[0].events = POLLIN;

	int ret = poll(fds, sizeof(fds)/sizeof(fds[0]), 1000);
	if (ret == 0)
	{
	  printf ("\rwaiting..%d\r", ++timeout_periods);
	  fflush(stdout);
	}
	else if ((ret > 0) && (fds[0].revents & POLLIN))
	{
	    struct dmx512frame frame;
	    bzero(&frame, sizeof(frame));
	    int n = read (dmxfd, &frame, sizeof(frame));
	    if (n > 0)
	    {
		printf ("[%d]\n", count++);
		printf (" port:%d\n", frame.port);
		printf (" breaksize:%d\n", frame.breaksize);
		printf (" startcode:%d\n", frame.startcode);
		printf (" #slots:%d\n", frame.payload_size);
		int i;
		for (i = 0; i < frame.payload_size && i < 20; ++i)
		  printf (" %02X", frame.data[1+i]);
		if (frame.payload_size > 20)
		  printf (" ...");
		printf ("\n");
	    }
	    else if (n == 0)
		printf ("no-frame\n");
	    else
	    {
		perror("read");
		break;
	    }
	    timeout_periods = 0;
	}
	else
	{
	    perror("poll");
	    break;
	}
    }
    close(dmxfd);
    return 0;
}
