
#include "pc16cx50_registers.h"
#include "pc16cx50.h"
#include "wbuart_16x50.h"

#include <stdio.h>
#include <unistd.h>


static void irqfunc(int irq, void *arg)
{
  (void)irq;
  struct pc16x50 * uart = (struct pc16x50 *)arg;

  const int isr = pc16x50_read_isr(uart);
  if ((isr&1) == 0)
    {
      printf ("IRQ ISR:%02X ", isr);
      switch (isr & PC16C550_IIR_IRQ_MASK)
        {
        case PC16C550_IIR_IRQ_RECEIVER_LINE_STATUS:
          printf("RLS\n");
          break;
        case PC16C550_IIR_IRQ_RECEIVED_DATA_AVAILABLE:
          printf("RX: %02X\n", pc16x50_read_rbr(uart));
          break;
        case PC16C550_IIR_IRQ_CHARACTER_TIMEOUT:
          printf("RXTIMEOUT: %02X\n", pc16x50_read_rbr(uart));
          break;
        case PC16C550_IIR_IRQ_TRANSMITTER_HOLDING_EMPTY:
          printf("THRE\n");
          break;
        case PC16C550_IIR_IRQ_MODEM_STATUS:
          printf("MODEM\n");
          break;
        case PC16C550_IIR_IRQ_NONE:
          break;
        default:
          printf ("???\n");
          break;
        }
    }
}

int main ()
{
  struct pc16x50 * uart = wbuart_pc16x50_create();
  wbuart_set_irqhandler(uart, irqfunc, uart);
  
  pc16x50_set_format (uart, 8, 'n', 2);
  pc16x50_set_baudrate_divisor  (uart, 25);
  pc16x50_set_ier_bits(uart,
                       PC16C550_IER_RECEIVED_DATA_AVAILABLE
                       | PC16C550_IER_TRANSMITTER_HOLDING_REGISTER_EMPTY
                       | PC16C550_IER_RECEIVER_LINE_STATUS
                       | PC16C550_IER_MODEM_STATUS
                       );

  // Do something
  pc16x50_set_fifo_trigger_level(uart, 4);

  // Start Transmision
  pc16x50_set_mcr_bits(uart, PC16550_MCR_RTS);

  // Send break
  pc16x50_set_baudrate_divisor  (uart, 62); // Break
  pc16x50_write_thr(uart, 0);
  while ((pc16x50_read_lsr(uart) & PC16550_LSR_TRANSMITTER_EMPTY) == 0);
  pc16x50_set_baudrate_divisor  (uart, 25); // Break

  // Send payload
  pc16x50_write_thr(uart, 0);
  pc16x50_write_thr(uart, 0);
  pc16x50_write_thr(uart, 0);
  pc16x50_write_thr(uart, 0);
  pc16x50_write_thr(uart, 0);
  pc16x50_write_thr(uart, 0);
  pc16x50_write_thr(uart, 0);
  pc16x50_write_thr(uart, 0);
  pc16x50_write_thr(uart, 0);
  pc16x50_write_thr(uart, 0);
  pc16x50_write_thr(uart, 0);

  // End transmision
  while ((pc16x50_read_lsr(uart) & PC16550_LSR_TRANSMITTER_EMPTY) == 0);
  pc16x50_set_mcr_bits(uart, PC16550_MCR_RTS);



  printf ("done\n");

  sleep(2);
  
  wbuart_delete(uart);
  return 0; 
}
