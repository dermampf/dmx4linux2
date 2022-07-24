#include "dmx512_suart.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

static void hexdump(char *prefix,
                    unsigned char * data,
                    int count,
                    char * postfix)
{
  int i;
  printf ("%s", prefix);
  for (i=0; i < count; ++i)
    printf (" %02X", data[i]);
  printf ("%s", postfix);
}


int main (int argc, char **argv)
{
    void dmx512_receive_frame(void * handle,
			      int flags,
                              unsigned char * dmx_data,
                              int dmxsize)
    {
        (void)handle;
      hexdump("DMX-Frame:", dmx_data, dmxsize, "\n");
    }

    struct dmx512suart_port * dmxport = dmx512suart_create_port (dmx512_receive_frame,
                                                                 0);

    if (argc > 1)
    {
	const int framecount = strtol(argv[1], 0, 0);
	int i;
	for (i = 0; i < framecount; ++i)
	{
	    unsigned char dmxdata[512];
	    int i;
	    for (i=0; i<512; ++i)
		dmxdata[i] = 0;

	    printf ("sending dmx\n");
	    dmx512suart_send_frame(dmxport,
				   0,
				   dmxdata,
				   64+1);
	    usleep(33*1000);
	}
    }
    else
    {
	printf ("changing dtr to provoke dmx-frame\n");
	// only toggle the RTS line : this is mapped to LED, which lets the dmx-device send a dmx-frame.
	int i;
	for (i=0; i < 8; ++i)
	{
	    dmx512suart_set_dtr (dmxport, 1);
	    usleep(500*1000);
	    dmx512suart_set_dtr (dmxport, 0);
	    usleep(500*1000);
	}
    }

    sleep(3);
    dmx512suart_delete_port (dmxport);

    return 0;
}
