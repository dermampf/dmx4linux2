#include "dmx512_suart.h"
#include <stdio.h>
#include <unistd.h>

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


int main ()
{
    void dmx512_receive_frame(void * handle,
                              unsigned char * dmx_data,
                              int dmxsize)
    {
        (void)handle;
      hexdump("DMX-Frame:", dmx_data, dmxsize, "\n");
    }

    struct dmx512suart_port * dmxport = dmx512suart_create_port (dmx512_receive_frame,
                                                                 0);

#if 0
    // Send dmx frame
    int k;
    for (k=0; k<4; ++k)
    {
        unsigned char dmxdata[512];
        int i;
        for (i=0; i<512; ++i)
            dmxdata[i] = 0;

        dmx512suart_send_frame(dmxport,
                               dmxdata,
                               64+1);


        usleep(500*1000);
    }
#else
    // only toggle the RTS line : this is mapped to LED, which lets the dmx-device send a dmx-frame.
    int k;
    for (k=0; k<8; ++k)
    {
        dmx512suart_set_dtr (dmxport, 1);
        usleep(500*1000);
        dmx512suart_set_dtr (dmxport, 0);
        usleep(500*1000);
    }
#endif

    //<<<< TO HERE >>>>>>

    sleep(3);

    dmx512suart_delete_port (dmxport);

    return 0;
}
