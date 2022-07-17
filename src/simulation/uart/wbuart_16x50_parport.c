
#include "wbuart_16x50.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>


#include <sys/ioctl.h>
#include <linux/parport.h>
#include <linux/ppdev.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <pthread.h>


struct pc16x50_epp
{
    struct pc16x50 uart;
    int fd;

    int run;
    pthread_t  irq_thread;

    void (*irqfunc)(int, void *);
    void * irqarg;
};


int EppWriteAddr (int parport, unsigned char *buff, size_t size)
{
  int mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
  if (ioctl(parport, PPSETMODE, &mode))
    return -1;
  return write (parport, buff, size);
}

int EppReadAddr (int parport, unsigned char *buff, size_t size)
{
  int mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
  if (ioctl(parport, PPSETMODE, &mode))
    return -1;
  return read (parport, buff, size);
}

int EppWriteData (int parport, unsigned char *buff, size_t size)
{
  int mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
  if (ioctl(parport, PPSETMODE, &mode))
    return -1;
  return write (parport, buff, size);
}

int EppReadData (int parport, unsigned char *buff, size_t size)
{
  int mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
  if (ioctl(parport, PPSETMODE, &mode))
    return -1;
  return read (parport, buff, size);
}



static void wbuart_reg_write(void * ctx, int reg, u8 value)
{
    printf ("wbuart_reg_write(reg:%02X, value:%02X\n", reg, value);
  struct pc16x50_epp * u = (struct pc16x50_epp *)ctx;
  unsigned char adr = reg;
  if (EppWriteAddr (u->fd, &adr, 1) < 0)
    return;
  EppWriteData (u->fd, &value, 1);
}

static u8 wbuart_reg_read(void * ctx, int reg)
{
    printf ("wbuart_reg_read(reg:%02X)\n", reg);
  struct pc16x50_epp * u = (struct pc16x50_epp *)ctx;
  unsigned char adr = reg;
  if (EppWriteAddr (u->fd, &adr, 1) < 0)
    return 0;
  u8 value;
  if (EppReadData (u->fd, &value, 1) < 0)
    return 0;
  return value;
}


// pthread function
void * interrupt_thread_function(void * arg)
{
    struct pc16x50_epp * u = (struct pc16x50_epp *)arg;

    while (1)
    {
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(u->fd, &rfds);
	if(select(u->fd + 1, &rfds, NULL, NULL, NULL))
	{
	    int int_count = 0;
	    printf("Received interrupt\n");

	    //Clear the interrupt
	    ioctl(u->fd, PPCLRIRQ, &int_count);
	    if(int_count > 1)
		printf("Uh oh, missed %i interrupts!\n", int_count - 1);

	    if  (u->irqfunc)
		u->irqfunc(0, u->irqarg);
	}
    }
}



struct pc16x50 * wbuart_pc16x50_create()
{
  const int pfd  = open("/dev/parport0", O_RDWR);
  if (pfd < 0)
  {
      printf ("no partport device\n");
      return 0;
  }
  if (ioctl(pfd, PPEXCL) < 0)
  {
      printf("no exclusive access possible\n");
  }
  if (ioctl(pfd, PPCLAIM) < 0)
  {
      printf ("failed to claim parport device\n");
      close(pfd);
      return 0;
    }

  struct pc16x50_epp * u = malloc(sizeof(struct pc16x50_epp));
  pc16x50_init(&u->uart, wbuart_reg_write, wbuart_reg_read, u);
  u->fd = pfd;

  u->run = 1;
  pthread_create(&u->irq_thread, NULL, interrupt_thread_function, u);

  // open UIO device and start pthread as irq handler.

  return &u->uart;
}

void wbuart_set_irqhandler(struct pc16x50 * u,
                           void (*irqfunc)(int, void *),
                           void * irqarg)
{
    struct pc16x50_epp * uart = (struct pc16x50_epp *)u;
    uart->irqfunc = irqfunc;
    uart->irqarg = irqarg;
}

void wbuart_delete(struct pc16x50 * u)
{
  if (u)
    {
      struct pc16x50_epp * uart = (struct pc16x50_epp *)u;


      uart->run = 0;
      pthread_join(uart->irq_thread, 0);

      ioctl(uart->fd, PPRELEASE);
      close(uart->fd);
      free(u);
    }
}

