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

    int count = 0;
    while (1)
    {
        fd_set  readset;
        FD_ZERO(&readset);
        FD_SET(dmxfd, &readset);
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        const int ret = select(dmxfd+1, &readset, 0, 0, &timeout);
        if (ret == 0)
            printf ("timeout\n");
        else if (ret < 0)
            exit(1);
        else
        {
            printf ("got something");
            struct dmx512frame frame;
            bzero(&frame, sizeof(frame));
            int n = read (dmxfd, &frame, sizeof(frame));
            printf ("[%d]\n", count++);
            printf (" port:%d\n", frame.port);
            printf (" breaksize:%d\n", frame.breaksize);
            printf (" startcode:%d\n", frame.startcode);
            printf (" #slots:%d\n", frame.payload_size);
        }
    }
    close(dmxfd);
    return 0;
}
