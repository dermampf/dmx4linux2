#include "uio.h"
#include <stdio.h>
#include <stdlib.h>

int main (int argc, char **argv)
{
    const char * uio_name = (argc > 1) ? argv[1] : 0;
    const int    addr     = (argc > 2) ? strtoul(argv[2], 0, 0) : -1;
    const int    value    = (argc > 3) ? strtoul(argv[3], 0, 0) : -1;
    if (uio_name)
    {
	struct uio_handle * uio;
	uio = uio_open (uio_name, 4096);
	if (!uio)
	    return 1;
	if (addr >= 0)
	{
	    if (value >= 0)
		uio_poke8(uio, addr, value);
	    else
		printf ("0x%X\n", uio_peek8 (uio, addr));
	    uio_close(uio);
	    return 0;
	}
	uio_close(uio);
    }
    printf ("usage: %s <uio-name> <addr> [<value>]\n", argv[0]);
    return 1;
}
