#include <stdint.h>
#include "streamuart.h"
#include <stdio.h>

#define offsetof(x)     (unsigned long)(&(((struct suart_event *)0)->x))
#define printsizeoffset(x,f) printf("sizeof(" #x "." #f "):%lu offset:%lu\n", sizeof(x.f), offsetof(f))
#define printsize(x) printf("sizeof(" #x "):%lu\n", sizeof(x))

int main ()
{
  struct suart_event e;

  printsize(e);

  printsizeoffset(e,event_type);

  printsizeoffset(e,data);
  printsizeoffset(e,data.data);
  printsizeoffset(e,data.count);

  printsizeoffset(e,data_inline);
  printsizeoffset(e,data_inline.count);
  printsizeoffset(e,data_inline.data);

  printsizeoffset(e,baudrate);

  printsizeoffset(e,format);
  printsizeoffset(e,format.databits);
  printsizeoffset(e,format.parity);
  printsizeoffset(e,format.stopbits);

  printsizeoffset(e,handshake);

  printsizeoffset(e,linemask);
  printsizeoffset(e,eventid);

  return 0;
}
