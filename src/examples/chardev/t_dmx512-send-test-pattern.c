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

static int generate_pattern(unsigned char * data)
{
    int i;
    for (i = 0; i < 513; ++i)
        data[i] = (unsigned char)i;
    return 513;
}

int main (int argc, char **argv)
{
    const char * card_name = (argc > 1) ? argv[1] : "/dev/dmx-card0";
    int dmxfd = open(card_name, O_RDWR);
    if (dmxfd < 0)
	return 1;

    struct dmx512frame frame;
    bzero(&frame, sizeof(frame));
    frame.port = (argc > 3) ? atoi(argv[3]) : 0;
    frame.breaksize = 0;
    frame.payload_size = generate_pattern(frame.data) - 1;
    write (dmxfd, &frame, sizeof(frame));

    close(dmxfd);

    return 0;
}
