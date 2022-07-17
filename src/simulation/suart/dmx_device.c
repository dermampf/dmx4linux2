#include "dmx_device.h"
#include "hardware_model.h"

#include <stdio.h>
#include <string.h>

static void setup_dmx_frame(struct dmx_device_s * dmx, int slot1)
{
  int i;
  unsigned char * p = dmx->dmxbits;
  // Break 00000000.00000000.00000011 4*22=88
  *(p++) = 0;
  *(p++) = 0;
  *(p++) = 0xC0;

  for (i = 0; i < 25; ++i)
    { // Startbyte:00 Slots:01,02,03,..
      unsigned char value = (i==1) ? slot1 : i;
      *(p++) = ((unsigned char)value)<<1;
      *(p++) = 0xfe + ((value&0x80)?1:0);
    }
  dmx->dmxbits_count = 8*(3+2+2*24);
}


// Called once every 5ns
int dmxDeviceHandler(void *arg, int txen, int tx, int led)
{
  int rx = 1;
  struct dmx_device_s * dmx = (struct dmx_device_s *)arg;
  ++dmx->counter_ns;

  if (dmx->enabled)
    {
      // 1000us / 25 / 16 / 2 / 5 = 4us
      if ((dmx->counter_ns % (25*16*2)) == 0)
        {
          if (dmx->bitcount > dmx->dmxbits_count)
              dmx->enabled = 0;
          else
              dmx->rx = (dmx->dmxbits[dmx->bitcount/8] >> (dmx->bitcount%8)) & 1;
          ++dmx->bitcount;
        }
      rx = dmx->rx;
    }
  else
    dmx->rx = 1;
  
  if (!dmx->last_led & led)
    {
      dmx->enabled = 1;
      dmx->bitcount = 0;
      ++dmx->slot1_value;
      setup_dmx_frame(dmx, dmx->slot1_value);
    }

  dmx->last_txen = txen;
  dmx->last_led  = led;
  return rx;
}


void init_dmx_device(struct dmx_device_s * dmx)
{
  int i;
  memset(dmx, 0, sizeof(dmx));
  memset(dmx->dmxbits, 0xff, sizeof(dmx->dmxbits));

  setup_dmx_frame(dmx, 0);

  wbBindDmxCallback(dmxDeviceHandler, dmx);
}
