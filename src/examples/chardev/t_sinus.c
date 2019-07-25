#include <linux/dmx512/dmx512frame.h>

#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static inline int max(int a, int b)
{
  return (a > b) ? a : b;
}

int main (int argc, char **argv)
{
    const char * card_name = (argc > 1) ? argv[1] : "/dev/dmx-card0";
    int dmxfd = open(card_name, O_RDWR);
    if (dmxfd < 0)
	return 1;

    struct dmx512frame frame;
    bzero(&frame, sizeof(frame));
    frame.port = (argc > 2) ? atoi(argv[2]) : 0;
    frame.breaksize = 0; // default
    frame.startcode = 0;
    frame.payload_size = 512;
    memset (frame.payload, 0, 512); // all lamps on

    const double stretchingFactor = (argc > 3) ? atof(argv[3]) : 0.2;
    const double speedFactor = (argc > 4) ? atof(argv[4]) : 0.15;

    int dmxframe = 0;
    while (1)
    {
	++dmxframe;

	usleep(1000*33);

	for (int i=0; i < 512; ++i)
	{
	    frame.payload[i] = max(0, (int)(sin(dmxframe*speedFactor + i*stretchingFactor) * 255.01));
	}
	write (dmxfd, &frame, sizeof(frame));

	printf ("frame:%d\r", dmxframe);
	fflush(stdout);
    }
    close(dmxfd);
    return 0;
}
