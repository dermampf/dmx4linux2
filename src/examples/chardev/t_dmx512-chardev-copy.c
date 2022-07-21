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
    if (argc != 5)
    {
        printf ("Copy frames from one dmx-cards port to a cards port.\n");
        printf ("The destination card can be the same as the source card.\n");
        printf ("%s <source-card> <source-port> <target-card> <target-port>\n", argv[0]);
        return 1;
    }
    const char * srccard = argv[1];
    const int    srcport = atoi(argv[2]);
    const char * dstcard = argv[3];
    const int    dstport = atoi(argv[4]);

    int srcdmxfd = open(srccard, O_RDWR);
    if (srcdmxfd < 0)
	return 1;

    int dstdmxfd = open(dstcard, O_RDWR);
    if (dstdmxfd < 0)
	return 1;

    unsigned long long portmask = 1<<srcport;
    if (ioctl(srcdmxfd, DMX512_IOCTL_SET_PORT_FILTER, &portmask))
    {
        printf("failed to set source filter\n");
        return 1;
    }

    while (1)
    {
        fd_set  readset;
        FD_ZERO(&readset);
        FD_SET(srcdmxfd, &readset);
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        const int ret = select(srcdmxfd+1, &readset, 0, 0, &timeout);
        if (ret < 0)
            break;
        else if ((ret > 1) && FD_ISSET(srcdmxfd, &readset))
        {
            struct dmx512frame frame;
            const int r = read(srcdmxfd, &frame, sizeof(frame));
            if (r == sizeof(frame))
            {
//                printf("copy frame from %s:%d to %s:%d with size %d\n", srccard, frame.port, dstcard, dstport, r);
                frame.port = dstport;
                write(dstdmxfd, &frame, sizeof(frame));
            }
            else
               printf("illegal size %d (expect:%d)\n", r, sizeof(frame));
        }
    }
    close(srcdmxfd);
    close(dstdmxfd);
    return 0;
}
