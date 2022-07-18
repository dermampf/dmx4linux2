// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 Michael Stickel <michael@cubic.org>
 */
#include <linux/dmx512/dmx512frame.h>

#include <sys/ioctl.h>
#include <linux/dmx512/dmx512_ioctls.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>


int main (int argc , char **argv)
{
    const char * card_name = (argc > 1) ? argv[1] : "/dev/dmx-card0";
    int dmxfd = open(card_name, O_RDWR);
    if (dmxfd < 0)
	return 1;

    unsigned long long portmask = (argc > 2) ? atoi(argv[2]) : 1;
    int ret = ioctl(dmxfd, DMX512_IOCTL_SET_PORT_FILTER, &portmask);
    printf ("DMX512_IOCTL_SET_PORT_FILTER -> %d\n", ret);

    portmask = 0;
    ret = ioctl(dmxfd, DMX512_IOCTL_GET_PORT_FILTER, &portmask);
    printf ("DMX512_IOCTL_GET_PORT_FILTER -> %d\n", ret);
    printf ("current port filter: %08llX\n", portmask);

    
    struct dmx512frame frame;
    bzero(&frame, sizeof(frame));
    int n = read (dmxfd, &frame, sizeof(frame));

    close(dmxfd);

    return 0;
}
