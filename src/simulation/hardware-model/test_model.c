// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#include "hardware_model.h"
#include <stdio.h>
#include <unistd.h>


void handleUartIrq(int irqno, void *arg)
{
  const int isr = wbReadMemory(2, 1);
  if ((isr&1) == 0)
    {
      printf ("ISR: %02X\n", isr);
      switch ((isr>>1)&7)
        {
        case 3: // RLS
          printf("RLS\n");
          break;
        case 2: // RX
          printf("RX: %02X\n", wbReadMemory(0, 1));
          break;
        case 6: // TO
          printf("TO: %02X\n", wbReadMemory(0, 1));
          break;
        case 1: // THRE
          printf("THRE\n");
          break;
        case 0: // MODEM
          printf("MODEM\n");
          break;
        }
    }
}

static void pc16c550_send_dmx()
{
  // 100MHz clock  Break 88us for 8bits / 11us/bit => 100MHz = 10ns/cycle / 16
  const double baseclock = 100000000.0; // 100MHz
  const double serialclock = baseclock / 16.0; // 16 times oversampling
  const double break_bittime = 11.0/1000000.0; // 88us / 8bits = 11us/bit
  const double dmx_bittime = 4.0/1000000.0; // 4us DMX bittime
  const int break_divisor = (int)((serialclock * break_bittime) + 0.9999);
  const int dmx_divisor = (int)((serialclock * dmx_bittime) + 0.9999);


  // Send break
  wbWriteMemory(3, 1, 0x80);
  wbWriteMemory(0, 1, break_divisor&0xff);
  wbWriteMemory(1, 1, (break_divisor>>8)&0xff);
  wbWriteMemory(3, 1, 7);
  wbWriteMemory(0, 1, 0);

  // Wait for octet to be send out

}


int main(int argc, char **argv)
{
  wbStartup(argc, argv);
  wbBindInterrupt(0, handleUartIrq, NULL);
  wbStart();

  printf ("requester\n");
  sleep(1);
  
  for (int i = 0; i <= 15; ++i)
    printf ("[%d]=%02X\n", i, wbReadMemory(i, 1));

  wbWriteMemory(3, 1, 0x80);
  wbWriteMemory(0, 1, 0x40);
  wbWriteMemory(1, 1, 0x0);
  wbWriteMemory(3, 1, 7);

  wbWriteMemory(1, 1, 15); // Enable RX Interrupt
  usleep(500*1000);
  wbWriteMemory(0, 1, 0xa1); // send 0xa1
  usleep(500*1000);
  wbWriteMemory(0, 1, 0xa2); // send 0xa2
  usleep(500*1000);
  wbWriteMemory(0, 1, 0xa3); // send 0xa3
  usleep(500*1000);
  wbWriteMemory(0, 1, 0xa4); // send 0xa4
  // wbWriteMemory(1, 1, 0); // THRE Irq

  usleep(500*1000);
  wbCleanup();

  return 0;
}
