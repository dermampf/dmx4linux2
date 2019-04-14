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

int main ()
{
    int dmxfd = open("/dev/dmx-dummy", O_RDWR);
    if (dmxfd < 0)
	return 1;

    int count = 0;
    while (1)
    {
	struct dmx512frame frame;
	bzero(&frame, sizeof(frame));
	int n = read (dmxfd, &frame, sizeof(frame));
	printf ("[%d]\n", count++);
	printf (" port:%d\n", frame.port);
	printf (" breaksize:%d\n", frame.breaksize);
	printf (" startcode:%d\n", frame.startcode);
	printf (" #slots:%d\n", frame.payload_size);
    }
    close(dmxfd);
    return 0;
}
