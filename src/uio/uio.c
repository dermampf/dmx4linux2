#include "uio.h"

#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>

void          uio_poke (struct uio_handle *h, unsigned long address, unsigned long value)
{
    if (h)
        ((unsigned long *)h->mem)[address/4] = value;
}

void          uio_poke16 (struct uio_handle *h, unsigned long address, unsigned short value)
{
    if (h)
        ((unsigned short *)h->mem)[address/2] = value;
}

void          uio_poke8 (struct uio_handle *h, unsigned long address, unsigned char value)
{
    if (h)
        ((unsigned char *)h->mem)[address] = value;
}

unsigned long  uio_peek (struct uio_handle * h, unsigned long address)
{
    if (h)
        return ((unsigned long *)h->mem)[address/4];
    return 0;
}

unsigned short uio_peek16 (struct uio_handle * h, unsigned long address)
{
    if (h)
        return ((unsigned short *)h->mem)[address/2];
    return 0;
}

unsigned char  uio_peek8 (struct uio_handle * h, unsigned long address)
{
    if (h)
        return ((unsigned char *)h->mem)[address];
    return 0;
}

struct   uio_handle * uio_open (const char * name, const unsigned long size)
{
  const int fd = open(name, O_RDWR /* | O_NONBLOCK */);
  if (fd < 0)
    {
      perror("open uio\n");
      return 0;
    }

  void * mem = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mem == MAP_FAILED)
    {
      close(fd);
      return 0;
    }
  struct uio_handle * h = malloc(sizeof(struct uio_handle));
  if (h)
  {
    h->mem = mem;
    h->fd = fd;
    h->size = size;
    return h;
  }
  munmap(mem, size);
  close(fd);
  return 0;
}

void          uio_close (struct uio_handle *h)
{
  if (h)
  {
    if (h->mem)
      munmap(h->mem, h->size);
    if (h->fd >= 0)
      close(h->fd);
    free(h);
  }
}


static void strip(char *s)
{
    if (s)
        s[strcspn(s, "\r\n")] = 0;
}


int iterate_uio(int (*iter) (int,const char*,const char*,void*), void * user)
{
  int i;
  struct dirent **namelist;
  int n = scandir("/sys/class/uio/", &namelist, 0, alphasort);
  if (n == -1)
    return 1;


  for (i = 0; i < n; ++i)
    {
      if (strncmp(namelist[i]->d_name, "uio", 3))
          continue;
      char x[100];
      snprintf(x, sizeof(x), "/sys/class/uio/%s/maps/map0/name", namelist[i]->d_name);

      char s[100];
      FILE * f = fopen(x, "r");
      if (fgets(s, sizeof(s), f))
        {
          strip(s);
          const int r = iter(i, namelist[i]->d_name, s, user);
          if (r != -1)
              return r;
        }
    }
  free(namelist);
  return -1;
}


void list_all_uios()
{
  int list_uio (int index,const char*uio,const char*name,void*user)
  {
    printf ("%s %s\n", uio, name);
    return -1;
  }
  iterate_uio(list_uio, NULL);
}


long uio_wait_for_interrupt_timeout(struct uio_handle * uio, struct timeval * timeout)
{
    long info;
    int    ret;
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(uio->fd, &rfds);
    ret = select(uio->fd + 1, &rfds, NULL, NULL, timeout);

    if (ret == 0) // timeout
      return -1;

    if (ret < 0) // error
      return ret;

    if (!FD_ISSET(uio->fd, &rfds)) // not us
      return -1;

    if (sizeof(info) == read(uio->fd, &info, sizeof(info)))
        return info;

    return -1;
}
