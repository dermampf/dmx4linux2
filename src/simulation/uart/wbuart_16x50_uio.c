#include "wbuart_16x50.h"
#include "uio.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>


#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <pthread.h>


struct pc16x50_uio
{
    struct pc16x50 uart;
    struct uio_handle * uio;

    int run;
    pthread_t  irq_thread;

    void (*irqfunc)(int, void *);
    void * irqarg;
};



static void wbuart_reg_write(void * ctx, int reg, u8 value)
{
    //fprintf (stderr, "wbuart_reg_write(reg:%02X, value:%02X\n", reg, value);
    struct pc16x50_uio * u = (struct pc16x50_uio *)ctx;
    uio_poke8(u->uio, reg, value);
}

static u8 wbuart_reg_read(void * ctx, int reg)
{
    struct pc16x50_uio * u = (struct pc16x50_uio *)ctx;
    const u8 value = uio_peek8 (u->uio, reg);
    //fprintf (stderr, "wbuart_reg_read(reg:%02X) => %02X\n", reg, value);
    return value;
}


static void * interrupt_thread_function(void * arg)
{
    struct pc16x50_uio * u = (struct pc16x50_uio *)arg;

    while (1)
    {
	struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

	if (!u->run)
	    return 0;

	uio_enable_interrupt(u->uio);
	if (uio_wait_for_interrupt_timeout(u->uio, &timeout) >= 0)
	{
	    if  (u->irqfunc)
		u->irqfunc(0, u->irqarg);
	}
    }
}



struct pc16x50 * wbuart_pc16x50_create_by_index(int index)
{
    struct uio_handle * uio = 0;

    char uio_name[128];

    if (index >= 0)
      snprintf(uio_name, sizeof(uio_name), "/dev/uio%d", index);
    else
      {
	const char * ext_uio_name = getenv("UIO_NAME");
	if ((ext_uio_name==0) || (strlen(ext_uio_name)>=sizeof(uio_name)))
	  return 0;
	strncpy(uio_name, ext_uio_name, sizeof(uio_name));
      }

    uio = uio_open (uio_name, 4096);
    if (!uio)
    {
	printf ("failed to claim parport device\n");
	return 0;
    }

    struct pc16x50_uio * u = malloc(sizeof(struct pc16x50_uio));
    pc16x50_init(&u->uart, wbuart_reg_write, wbuart_reg_read, u);
    u->uio = uio;

    u->run = 1;
    pthread_create(&u->irq_thread, NULL, interrupt_thread_function, u);

    return &u->uart;
}

struct pc16x50 * wbuart_pc16x50_create()
{
  return wbuart_pc16x50_create_by_index(-1);
}

void wbuart_set_irqhandler(struct pc16x50 * u,
                           void (*irqfunc)(int, void *),
                           void * irqarg)
{
    struct pc16x50_uio * uart = (struct pc16x50_uio *)u;
    uart->irqfunc = irqfunc;
    uart->irqarg = irqarg;
}

void wbuart_delete(struct pc16x50 * u)
{
  if (u)
    {
      struct pc16x50_uio * uart = (struct pc16x50_uio *)u;

      uio_disable_interrupt(uart->uio);

      uart->run = 0;
      pthread_join(uart->irq_thread, 0);

      uio_close(uart->uio);

      free(u);
    }
}
