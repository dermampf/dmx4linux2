//https://www.kernel.org/doc/htmldocs/kernel-api/kfifo.html

#include "kfifo/kfifo.h"
#include <stdio.h>


struct element
{
  int a;
  int b;
};

#define FRAMECOUNT 1

struct A
{
  DECLARE_KFIFO(txfifo, struct element, 64*FRAMECOUNT);
  DECLARE_KFIFO(rxfifo, struct element, 64*FRAMECOUNT);
};

struct A * createA()
{
  struct A * a = malloc(sizeof(struct A));
  INIT_KFIFO(a->txfifo);
  INIT_KFIFO(a->rxfifo);
  return a;
}

int main ()
{
  struct A * a = createA();

  const struct element x[2] = { {1,2}, {3,4} };
  printf ("%d\n", kfifo_in(&a->txfifo, x, 2));

  printf ("len:%d\n", kfifo_peek_len(&a->txfifo));
  {
    struct element y[3];
    const int n2 = kfifo_out(&a->txfifo, y, 3);
    printf ("%d\n", n2);
    int i;
    for (i = 0; i < n2; ++i)
      printf ("%d/%d\n", y[i].a, y[i].b);
  }

  return 0;
}
