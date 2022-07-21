#include <linux/dmx512/dmx512frame.h>

#include <sys/ioctl.h>
#include <linux/dmx512/dmx512_ioctls.h>

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>



typedef unsigned char dmx_t;
struct dmx_device
{
  int fd;
};

struct dmx_device * open_dmx_device()
{
  const int fd = open("/dev/dmx-card0", O_RDWR|O_NONBLOCK);
  if (fd < 0)
    return 0;
  struct dmx_device * dmxdev = (struct dmx_device *)malloc(sizeof(*dmxdev));
  if (dmxdev)
    {
      dmxdev->fd = fd;
      unsigned long long portmask = 1;
      ioctl(fd, DMX512_IOCTL_SET_PORT_FILTER, &portmask);
    }
  return dmxdev;
}

void close_dmx_device(struct dmx_device * dmxdev)
{
  if (dmxdev)
    {
      close(dmxdev->fd);
      free(dmxdev);
    }
}

int dmx_device_port_count(struct dmx_device *dmxdev)
{
  (void)dmxdev;
  return 1;
}

void dmx_device_send_frame(struct dmx_device *dmxdev,
			   int port,
			   dmx_t startcode,
			   dmx_t * payload,
			   int payload_size)
{
    struct dmx512frame frame;
    bzero(&frame, sizeof(frame));
    frame.port = port;
    frame.breaksize = startcode;
    frame.startcode = startcode;
    frame.payload_size = payload_size;
    memcpy (frame.payload, payload, payload_size);
    write (dmxdev->fd, &frame, sizeof(frame));
}

int dmx_device_fd(struct dmx_device *dmxdev)
{
  return dmxdev->fd;
}

int dmx_device_read_rxframe(struct dmx_device *dmxdev,
			    int * port,
			    dmx_t * startcode,
			    dmx_t * payload)
{
    struct dmx512frame frame;
    const int size = read(dmxdev->fd, &frame, sizeof(frame));
    if (size == sizeof(frame))
      {
	*port = frame.port;
	*startcode = frame.startcode;
	memcpy (payload, frame.payload, 512);
	return frame.payload_size - 1;
      }
    return 0;
}
