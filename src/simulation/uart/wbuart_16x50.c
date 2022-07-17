
#include "wbuart_16x50.h"

#include "hardware_model.h"
#include <stdlib.h>
#include <string.h>

static void wbuart_reg_write(void * ctx, int reg, u8 value)
{
  (void)ctx;
  wbWriteMemory(reg, 1, value);
}

static u8 wbuart_reg_read(void * ctx, int reg)
{
  (void)ctx;
  return (u8)wbReadMemory(reg, 1);
}

#include "../suart/dmx_device.h"
struct dmx_device_s dmx;

struct pc16x50 * wbuart_pc16x50_create()
{
  struct pc16x50 * u = malloc(sizeof(struct pc16x50));
  pc16x50_init(u, wbuart_reg_write, wbuart_reg_read, 0);
  wbStartup(0, 0);
  init_dmx_device(&dmx);
  wbStart();
  return u;
}

void wbuart_set_irqhandler(struct pc16x50 * u,
                           void (*irqfunc)(int, void *),
                           void * irqarg)
{
  (void)u;
  wbBindInterrupt(0, irqfunc, irqarg);
}

void wbuart_delete(struct pc16x50 * u)
{
  if (u)
    {
      wbCleanup();
      free(u);
    }
}
