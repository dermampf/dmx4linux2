#include <linux/dmx512/dmx512frame.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

/*
 * send out one dmx-frame to all cards
 * If you open /dev/dmx512/card0port0 that frame is output only to that port.
 */

int main (int argc, char **argv)
{
    int dmxfd = open("/dev/dmx-dummy", O_RDWR);
    if (dmxfd < 0)
	return 1;
    const int count = (argc > 1) ? atoi(argv[1]) : 10;

    int i;
    for (i = 0; i < count; ++i)
    {
	struct dmx512frame frame;
	bzero(&frame, sizeof(frame));
	frame.port = 1; // any port.
	frame.breaksize = 0; // default
	frame.startcode = 0;
	frame.payload_size = 512;
	memset (frame.payload, 0xff, 512); // all lamps on
	write (dmxfd, &frame, sizeof(frame));
    }

    close(dmxfd);

    return 0;
}
