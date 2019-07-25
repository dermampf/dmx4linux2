#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef unsigned long  u32;
typedef unsigned short u16;
typedef unsigned char  u8;


struct rtuart_pl011
{
	struct {
		u8 fifo[64];
		u8 inptr;
		u8 outptr;
	} rx;
};

static inline int pl011_rx_fifo_full(struct rtuart_pl011 * pl011)
{
	return ((pl011->rx.inptr+1)&63) == pl011->rx.outptr;
}

static inline int pl011_rx_fifo_empty(struct rtuart_pl011 * pl011)
{
	return pl011->rx.inptr == pl011->rx.outptr;
}

static inline int pl011_rx_fifo_level(struct rtuart_pl011 * pl011)
{
/*
	if (in == out) size = 0;
	if (in > out)  size = in - out;
	if (in < out)  size = (in+fifosize) - out;
*/
	if (pl011_rx_fifo_empty(pl011))
		return 0;

	if (pl011->rx.inptr > pl011->rx.outptr)
		return pl011->rx.inptr - pl011->rx.outptr;

	return (pl011->rx.inptr + sizeof(pl011->rx.fifo)) - pl011->rx.outptr;
}

static void pl011_rx_fifo_put(struct rtuart_pl011 * pl011, u8 v)
{
	if (!pl011_rx_fifo_full(pl011)) {
		const int p = pl011->rx.inptr;
		pl011->rx.fifo[p] = v;
		pl011->rx.inptr = (p+1) & 63;
	}
}

static int pl011_rx_fifo_get(struct rtuart_pl011 * pl011, u8 * v)
{
	if (!pl011_rx_fifo_empty(pl011)) {
		const int p = pl011->rx.outptr;
		*v = pl011->rx.fifo[p];
		pl011->rx.outptr = (p+1) & 63;
		return 1;
	}
	return 0;
}



static void status(struct rtuart_pl011 * pl011)
{
  printf("f:%d e:%d l:%d  in:%d out:%d\n",
	 pl011_rx_fifo_full(pl011),
	 pl011_rx_fifo_empty(pl011),
	 pl011_rx_fifo_level(pl011),
	 pl011->rx.inptr,
	 pl011->rx.outptr);
}


int main ()
{
  int i;
  struct rtuart_pl011 u;
  memset(&u, 0, sizeof(u));

  status(&u);
  for (i = 0; i < 32; ++i)
    {
      pl011_rx_fifo_put(&u, 1);
      status(&u);
    }

  u8 v;
  while (pl011_rx_fifo_get(&u, &v));
  status(&u);

  for (i = 0; i < 64; ++i)
    {
      pl011_rx_fifo_put(&u, 1);
      status(&u);
    }


  return 0;
}
