#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#include "rtuart_ioctl.h"

static void set_format(int fd, const int bits, const int parity, const int stop)
{
  unsigned long v = (bits<<16) | (parity<<8) | stop;
  ioctl(fd, RTUART_IOCTL_S_FMT, &v);
}

static void set_baudrate(int fd, unsigned long baudrate)
{
    ioctl(fd, RTUART_IOCTL_S_BAUDRATE, &baudrate);
}

static unsigned long get_baudrate(int fd)
{
    unsigned long baudrate;
    ioctl(fd, RTUART_IOCTL_G_BAUDRATE, &baudrate);
    return baudrate;
}

void rtuart_set_control(int fd, const unsigned long to_set)
{
    ioctl(fd, RTUART_IOCTL_S_CONTROL, &to_set);
}

void rtuart_clr_control(int fd, const unsigned long to_clr)
{
    ioctl(fd, RTUART_IOCTL_C_CONTROL, &to_clr);
}


void rtuart_set_notify(int fd, const unsigned long notify_mask)
{
    ioctl(fd, RTUART_IOCTL_S_NOTIFY, &notify_mask);
}

void rtuart_clr_notify(int fd, const unsigned long notify_mask)
{
    ioctl(fd, RTUART_IOCTL_C_NOTIFY, &notify_mask);
}


int main ()
{
	int fd = open("/dev/uart0", O_RDWR);
	if (fd == -1) {
		perror("open /dev/uart0");
		return 1;
	}

	set_format(fd, 8,'n',2);
	set_baudrate(fd, 250000);
	printf ("baudrate:%lu\n", get_baudrate(fd));

	rtuart_set_control(fd, RTUART_CONTROL_BREAK);
	usleep(88);
	rtuart_clr_control(fd, RTUART_CONTROL_BREAK);

	rtuart_set_notify(fd, RTUART_NOTIFY_TRANSMITTER_EMPTY);
	// RTUART_NOTIFY_TRANSMITTER_READY
	// RTUART_NOTIFY_TRANSMITTER_EMPTY
	// RTUART_NOTIFY_RECEIVER_HASDATA
	// RTUART_NOTIFY_RECEIVER_TIMEOUT
	// RTUART_NOTIFY_RECEIVER_EVENT


	unsigned char dmx_data[513] = {0,};
	write(fd, dmx_data, sizeof(dmx_data));

	rtuart_clr_notify(fd, RTUART_NOTIFY_TRANSMITTER_EMPTY);

	rtuart_set_notify(fd, RTUART_NOTIFY_RECEIVER_HASDATA|RTUART_NOTIFY_RECEIVER_TIMEOUT);

	int index = 0;
	int run = 1;
	while (run) {
		struct rtuart_read_event e;
		int n = read(fd, &e, sizeof(e));
		if (n == sizeof(e)) {
			printf ("[%d] event.what:%d : ", ++index, e.what);
			switch(e.what) {
			case RTUART_WHAT_CALLBACK_TX_START:
				printf ("CALLBACK_TX_START\n");
				break;
			case RTUART_WHAT_CALLBACK_TX_END1:
				printf ("CALLBACK_TX_END1\n");
				break;
			case RTUART_WHAT_CALLBACK_TX_END2:
				printf ("CALLBACK_TX_END2\n");
				run = 0;
				break;
			case RTUART_WHAT_CALLBACK_RX_ERR:
				printf ("CALLBACK_RX_ERR\n");
				break;
			case RTUART_WHAT_CALLBACK_RX_CHAR:
				printf ("CALLBACK_RX_CHAR\n");
				break;
			case RTUART_WHAT_CALLBACK_RX_END:
				printf ("CALLBACK_RX_END\n");
				break;
			case RTUART_WHAT_CALLBACK_RX_TIMEOUT:
				printf ("CALLBACK_RX_TIMEOUT\n");
				break;
			case RTUART_WHAT_CALLBACK_RX_TRIGGER:
				printf ("CALLBACK_RX_TRIGGER: mask=%08lX\n",
					e.rx_trigger.trigger_mask
					);
				break;
			default:
				printf("\n");
				break;
			}
		}
	}
	
	close(fd);
	return 0;
}
