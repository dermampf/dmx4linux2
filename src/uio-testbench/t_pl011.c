// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#include "uio32_bus.h"
#include "rtuart.h"
#include "pl011.h"
#include "kernel.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <poll.h> // poll
#include <math.h>

#include "rtuart_client.h"
#include "rtuart_dmx512_client.h"

#include "dmx512_port.h"

#include <unistd.h>


void dump_dmx512_frame(struct dmx512_framequeue_entry * frame, const char * prompt)
{
	printf ("%s frame:%p port:%d break:%d size:%d sc:%02X",
		prompt,
		frame,
		frame->frame.port,
		frame->frame.breaksize,
		frame->frame.payload_size,
		frame->frame.startcode
		);
	int i;
	for (i = 0; i < frame->frame.payload_size && i < 64; ++i)
	{
		if (i%16 == 0)
			printf ("\n%03X :", i);
		printf (" %02X", frame->frame.payload[i]);
	}
	if (i < frame->frame.payload_size)
		printf ("\n .....");
	printf ("\n");
}


struct dmx512_framequeue_entry * dmx512_get_frame(struct dmx512_port *port)
{
	(void)port;
	struct dmx512_framequeue_entry * f = (struct dmx512_framequeue_entry *)malloc(sizeof(*f));
	if (f)
		memset(f, 0, sizeof(*f));
	return f;
}

void dmx512_put_frame(struct dmx512_port *port, struct dmx512_framequeue_entry * frame)
{
	(void)port;
	if (frame)
		free(frame);
}

int dmx512_send_frame (struct dmx512_port * port, struct dmx512_framequeue_entry * frame)
{
	if (port->send_frame)
		return port->send_frame (port, frame);
	return -1;
}


struct dmx512_framequeue_entry * received_frames = 0;
static int received_dmx_signal = -1;
static unsigned long ok_counter = 0;
int dmx512_received_frame(struct dmx512_port *port, struct dmx512_framequeue_entry * frame)
{
	if (!frame)
		return 0;
	// dump_dmx512_frame(frame, "received ");
	//dmx512_put_frame(port, frame);

	// write frame to some queue.
	frame->head = received_frames;
	received_frames = frame;
	write(received_dmx_signal, "", 1);
}


static int max (const int a, const int b) { return (a>b) ? a : b; }

static int uio_handle_irq(int fd)
{
	u32 info;
	const int nb = read(fd, &info, sizeof(info));
	if (nb == (ssize_t)sizeof(info))
	{
		return 1;
		printf("Interrupt #%lu!\n", info);
	}
	return 0;
}

static void uio_set_interrupt_enable(const int fd, const int on)
{
        u32 info = on; /* 1=unmask, 0=mask */
        ssize_t nb = write(fd, &info, sizeof(info));
        if (nb != (ssize_t)sizeof(info)) {
		perror("write");
		close(fd);
		exit(EXIT_FAILURE);
        }
}
extern void pl011_periodic_check(struct rtuart * u);
void dmx512_rtuart_periodical(struct rtuart * uart);

int main (int argc, char **argv)
{
  const char * dmx_cardname = "/dev/dmx-card1";
  int dmxfd = open(dmx_cardname, O_RDWR | O_NONBLOCK);
  if (dmxfd < 0)
	  return 1;
  const int dmxport_id = (argc > 2) ? atoi(argv[2]) : 0;

  const char * uio_device = (argc > 1) ? argv[1] : "/dev/uio0";
  int fd;
  if ((fd = open(uio_device, O_RDWR)) == -1)
    {
      perror("open uio");
      return 1;
    }

  int rxdmx_pipe[2];
  if (pipe(rxdmx_pipe))
    {
      perror("pipe");
      exit(1);
    }
  received_dmx_signal = rxdmx_pipe[1];

  struct rtuart_bus * bus = uio32_create(fd, /*offset:*/0, /*size:*/0x1000);
  if (!bus)
    return 1;
  struct rtuart * uart = pl011_create(48000000); // 48MHz
  if (!uart)
    return 1;
  uart->bus = bus;
  struct dmx512_port * dmxport = dmx512_rtuart_client_create(uart);

  const double stretchingFactor = /*(argc > 2) ? atof(argv[2]) : */ 0.2;
  const double speedFactor = /* (argc > 3) ? atof(argv[3]) : */ 0.15;

  int framecounter = 0;

  struct timespec last;
  bzero(&last, sizeof(last));
  clock_gettime(CLOCK_MONOTONIC, &last);

  uio_set_interrupt_enable(fd, 1);
  while (1)
  {
	  struct pollfd fds[4];
	  memset (fds, 0, sizeof (fds));
	  fds[0].fd = fd;
	  fds[0].events = POLLIN;
	  fds[1].fd = 0;
	  fds[1].events = POLLIN;
	  fds[2].fd = dmxfd;
	  fds[2].events = POLLIN;
	  fds[3].fd = rxdmx_pipe[0];
	  fds[3].events = POLLIN;
	  int ret = poll(fds, sizeof(fds)/sizeof(fds[0]), 0);

	  if (ret < 0)
		  return ret;
	  else if (ret == 0)
	  {
		  pl011_periodic_check(uart);
		  dmx512_rtuart_periodical(uart);
		  continue;
	  }

	  if (fds[0].revents == POLLIN)
	  {
		  if (uio_handle_irq(fds[0].fd) && uart->ops->handle_irq)
		  {
			  rtuart_handle_irq(uart);
			  uio_set_interrupt_enable(fd, 1);
		  }
	  }

	  if (fds[1].revents == POLLIN)
	  {
		  char buffer[200];
		  if (fgets(buffer, sizeof(buffer), stdin))
		  {
			  if (!dmx512_rtuart_client_is_busy(uart))
			  {
				  struct dmx512_framequeue_entry * frame = dmx512_get_frame(dmxport);
				  if (frame)
				  {
					  frame->frame.payload_size = 512;
					  frame->frame.startcode = 0;
					  u8 * dmx_data = frame->frame.payload;
					  int i;
					  for (i=0; i < 512; ++i)
					  {
#if 0
						  if (framecounter&1)
							  dmx_data[i] = (u8)i;
						  else
							  dmx_data[i] = (u8)(511-i);
#else
						  dmx_data[i] = max(0, (int)(sin(framecounter*speedFactor + i*stretchingFactor) * 255.01));
#endif
					  }
					  dmx512_send_frame (dmxport, frame);
				  }
				  ++framecounter;
				  printf ("%d\n", framecounter);
			  }
			  else
				  printf ("Busy\n");
		  }
	  }

#if 1
	  if (fds[2].revents == POLLIN) // dmx-dummy
	  {
		  static unsigned int ingress_frame_count = 0;
		  struct dmx512frame f;
		  int n = read (dmxfd, &f, sizeof(f));
		  if (n == sizeof(f))
		  {
			  struct dmx512_framequeue_entry * frame = dmx512_get_frame(dmxport);
			  if (frame)
			  {
				  memcpy (&frame->frame, &f, sizeof(f));
				  printf ("[%d]\n", ingress_frame_count++);
				  //dump_dmx512_frame(frame, "forward ");

				  if ((frame->frame.port == dmxport_id) &&
				      !dmx512_rtuart_client_is_busy(uart))
					  dmx512_send_frame (dmxport, frame);
				  else
				  {
					  printf ("XXXX  port:%u expect:%u   busy:%d\n",
						  frame->frame.port,
						  dmxport_id,
						  dmx512_rtuart_client_is_busy(uart)
						  );
					  
					  dmx512_put_frame(dmxport, frame);
				  }
			  }
		  }
	  }
	  if (fds[3].revents == POLLIN) // dmx-receive data available
	  {
		  char c;
		  int n = read (rxdmx_pipe[0], &c, sizeof(c));
		  // if (n > 0) printf ("received-dmx-frame\n");

		  struct dmx512_framequeue_entry * f = received_frames;
		  received_frames = 0;

		  while (f)
		  {
			  struct dmx512_framequeue_entry * next = f->head;
			  f->head = 0;
			  
			  const int n = write (dmxfd, &f->frame, sizeof(f->frame));
			  if (n == sizeof(f->frame))
			  {
				  /*
				    printf ("send one frame to loopback port_%u size:%d\n",
				    f->frame.port,
				    f->frame.payload_size
				    );
				  */
				  dmx512_put_frame(dmxport, f);
			  }
			  f = next;
		  }
	  }
#endif
  }
  uio_set_interrupt_enable(fd, 0);

  return 0;
}
