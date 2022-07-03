#ifndef HARDWARE_MODEL_H
#define HARDWARE_MODEL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

  void wbStartup(int argc, char **argv);

  void wbCleanup();

  void wbWriteMemory(uint32_t address,
                     uint8_t  mask,
                     uint32_t data);

  uint32_t wbReadMemory(uint32_t address,
                        uint8_t  mask);

  void wbBindInterrupt(int irqno,
                       void (*irqfunc)(int irqno, void *arg),
                       void * arg);

#ifdef __cplusplus
}
#endif

#endif
