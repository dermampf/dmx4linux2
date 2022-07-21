/* you should make the test program with "make test CLFAGS=-DDOJDEBUG=1" */

#include "pointer.h"

int main(int argc, const char **argv)
{
  if(argc<2 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))
    {
      printf("usage: test [--js dev] [--mm dev] [--ms dev] [--pc dev] [--ps2 dev] [--gpm] [--mousescale factor]");
      printf("devices: js - joystick\n");
      printf("         mm - MM mouse protocol on rs232\n");
      printf("         ms - Microsoft mouse protocol on rs232\n");
      printf("         pc - MouseSystems (PC) protocol on rs232\n");
      printf("         gpm - GPM mouse server\n");
      printf("         ps2 - mouse on PS/2 bus\n");
      return 1;
    }
  if(pointerinit(&argc, argv) <= 0)
    return 1;
  while(1)
    {
      pointerprocess();
      printf("x:%+04i y:%+04i z:%+04i b:%+02i\n", pointerX(), pointerY(), pointerZ(), pointerB());
    }
  return 0;
}
