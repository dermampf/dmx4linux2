
#include "wbuart_16x50.h"

#include <stdlib.h>
#include <string.h>


#include <sys/ioctl.h>
#include <linux/parport.h>
#include <linux/ppdev.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>


struct pc16x50_epp
{
  struct pc16x50 uart;
  int fd;
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
  struct pc16x50_epp * u = (struct pc16x50_epp *)ctx;
  unsigned char adr = reg;
  if (EppWriteAddr (u->fd, &adr, 1) < 0)
    return;
  EppWriteData (u->fd, &value, 1);
}

static u8 wbuart_reg_read(void * ctx, int reg)
{
  struct pc16x50_epp * u = (struct pc16x50_epp *)ctx;
  unsigned char adr = reg;
  if (EppWriteAddr (u->fd, &adr, 1) < 0)
    return 0;
  u8 value;
  if (EppReadData (u->fd, &value, 1) < 0)
    return 0;
  return value;
}

struct pc16x50 * wbuart_pc16x50_create()
{
  const int pfd  = open("/dev/parport0", O_RDWR);
  if ((ioctl(pfd, PPEXCL) < 0) || (ioctl(pfd, PPCLAIM) < 0))
    {
      close(pfd);
      return 0;
    }

  struct pc16x50_epp * u = malloc(sizeof(struct pc16x50_epp));
  pc16x50_init(&u->uart, wbuart_reg_write, wbuart_reg_read, u);
  u->fd = pfd;

  // open UIO device and start pthread as irq handler.

  return &u->uart;
}

void wbuart_set_irqhandler(struct pc16x50 * u,
                           void (*irqfunc)(int, void *),
                           void * irqarg)
{
  (void)u;
}

void wbuart_delete(struct pc16x50 * u)
{
  if (u)
    {
      struct pc16x50_epp * uart = (struct pc16x50_epp *)u;
      ioctl(uart->fd, PPRELEASE);
      close(uart->fd);
      free(u);
    }
}

