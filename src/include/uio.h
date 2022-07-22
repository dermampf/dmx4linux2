#ifndef DEFINED_UIO_H
#define DEFINED_UIO_H

#include <sys/types.h>

struct uio_handle
{
  void *  mem;
  size_t  size;
  int     fd;
};

void          uio_poke (struct uio_handle *h, unsigned long address, unsigned long value);
void          uio_poke16 (struct uio_handle *h, unsigned long address, unsigned short value);
void          uio_poke8 (struct uio_handle *h, unsigned long address, unsigned char value);

unsigned long  uio_peek (struct uio_handle * h, unsigned long address);
unsigned short uio_peek16 (struct uio_handle * h, unsigned long address);
unsigned char  uio_peek8 (struct uio_handle * h, unsigned long address);

struct        uio_handle * uio_open (const char * name, const unsigned long size);
void          uio_close (struct uio_handle *h);

int iterate_uio(int (*iter) (int index, const char * uio, const char *name, void *user), void * user);

void list_all_uios();


#include <unistd.h>

static int uio_enable_interrupt(struct uio_handle * uio)
{
    long enable=1;
    return write(uio->fd, &enable, sizeof(enable));
}

static int uio_disable_interrupt(struct uio_handle * uio)
{
    long enable=0;
    return write(uio->fd, &enable, sizeof(enable));
}

static long uio_wait_for_interrupt(struct uio_handle * uio)
{
    long info;
    if (sizeof(info) == read(uio->fd, &info, sizeof(info)))
        return info;
    return -1;
}

long uio_wait_for_interrupt_timeout(struct uio_handle * uio, struct timeval * timeout);


#endif
