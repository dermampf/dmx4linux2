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

#include <assert.h>

const int MAX_FDS = 32;

int main (int argc , char **argv)
{
    const char * card_name = (argc > 1) ? argv[1] : "/dev/dmx-card0";

    int i;
    for (i = 0; i < MAX_FDS+1; ++i)
    {
        const int fd = open(card_name, O_RDWR);
        if (fd < 0)
        {
            printf ("-------\n");
            perror("open");
        }
        printf ("%d: %d\n", i, fd);
    }

    return 0;
}
