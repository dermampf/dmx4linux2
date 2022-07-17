// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#ifndef HARDWARE_MODEL_H
#define HARDWARE_MODEL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

  void wbStartup(int argc, char **argv);

  void wbStart();

  void wbCleanup();

  void wbWriteMemory(uint32_t address,
                     uint8_t  mask,
                     uint32_t data);

  uint32_t wbReadMemory(uint32_t address,
                        uint8_t  mask);

  void wbBindInterrupt(int irqno,
                       void (*irqfunc)(int irqno, void *arg),
                       void * arg);

  void wbBindDmxCallback(int (*handler)(void *arg, int txen, int tx, int led),
                         void * arg);

#ifdef __cplusplus
}
#endif

#endif
