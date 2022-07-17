#ifndef DEFINED_DMX_DEVICE
#define DEFINED_DMX_DEVICE

struct dmx_device_s
{
  unsigned long long counter_ns;
  int  last_txen;
  int  last_led;
  int  enabled;
  int  bitcount;
  unsigned char slot1_value;
  int rx;
  int dmxbits_count;
  unsigned char  dmxbits[128];
};
void init_dmx_device(struct dmx_device_s * dmx);

#endif
