// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
/*
 * This is a very very very simple commandline to access the registers of the simulated uart.
 *     <addr>           - read register at hex address <addr>.
 *     <addr> <value>   - write hex value <value> to register at hex address <addr>.
 *     <addr>=<value>   - write hex value <value> to register at hex address <addr>.
 *     <addr>|=value    - OR hex value <value> to register at hex address <addr>.
 *     <addr>&=value    - AND hex value <value> to register at hex address <addr>.
 *     <addr>&=~<value> - NAND hex value <value> to register at hex address <addr>.
 *
 * It also registers an interrupt handler and outputs the status of the ISR.
 */
#include "hardware_model.h"
#include <stdio.h>
#include <unistd.h>


void handleUartIrq(int irqno, void *arg)
{
  const int isr = wbReadMemory(2, 1);
  if ((isr&1) == 0)
    {
      printf ("IRQ ISR:%02X ", isr);
      switch ((isr>>1)&7)
        {
        case 3: // RLS
          printf("RLS\n");
          break;
        case 2: // RX
          printf("RX: %02X\n", wbReadMemory(0, 1));
          break;
        case 6: // TO
          printf("RXTIMEOUT: %02X\n", wbReadMemory(0, 1));
          break;
        case 1: // THRE
          printf("THRE\n");
          break;
        case 0: // MODEM
          printf("MODEM\n");
          break;
        default:
          printf ("???\n");
          break;
        }
    }
}

int main(int argc, char **argv)
{
  wbStartup(argc, argv);
  wbBindInterrupt(0, handleUartIrq, NULL);

  char s[100];
  while (fgets(s, sizeof(s), stdin))
    {
      int addr = 0, value = 0;
      if ((sscanf(s, "%X %X", &addr, &value)==2) || (sscanf(s, "%X=%X", &addr, &value)==2))
        {
          printf ("[%X]:=%X\n", addr, value);
          wbWriteMemory(addr, 1, value);
        }
      else if (sscanf(s, "%X|=%X", &addr, &value)==2)
        {
          printf ("[%X]|=%X\n", addr, value);
          wbWriteMemory(addr, 1, wbReadMemory(addr, 1) | value);
        }
      else if (sscanf(s, "%X&=%X", &addr, &value)==2)
        {
          printf ("[%X]&=%X\n", addr, value);
          wbWriteMemory(addr, 1, wbReadMemory(addr, 1) & value);
        }
      else if (sscanf(s, "%X&=~%X", &addr, &value)==2)
        {
          printf ("[%X]&=~%X\n", addr, value);
          wbWriteMemory(addr, 1, wbReadMemory(addr, 1) & ~value);
        }
      else if (sscanf(s, "%X", &addr)==1)
        {
          printf ("[%X] => %X\n", addr, wbReadMemory(addr, 1));
        }
    }

  wbCleanup();

  return 0;
}
