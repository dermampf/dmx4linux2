#include "streamuart.h"
#include "streamuart_event.h"
//#include "kfifo.h"

//#define REAL_UART

#include "../uart/pc16cx50_registers.h"
#include "../uart/pc16cx50.h"


#include <stdio.h>
#include <unistd.h>
#include <string.h>

static const int debug = 0;

static void hexdump(char *prefix,
                    u8 * data,
                    int count,
                    char * postfix)
{
  int i;
  printf ("%s", prefix);
  for (i=0; i < count; ++i)
    printf (" %02X", data[i]);
  printf ("%s", postfix);
}



const int WaitForReceiveDataTimeout_ms = 20;
const int UartIdleTimeout_ms = 2000;

struct suart_pc16x50
{
  struct suart_s     suart;

  wait_queue_head_t  txwaitqueue;
  wait_queue_head_t *rxwaitqueue;
  pthread_t          suart_handler_thread;
  struct pc16x50 *   uart;
  unsigned long      base_clock;
  int                rxenabled;
  DECLARE_KFIFO(txbuffer, u8, 1024);
  DECLARE_KFIFO(rxbuffer, u8, 1024);
};


static u8 suart_line_to_mcr(int v)
{
  u8 x = 0;
  if (v & SUART_LINE_RTS)
    x |= PC16550_MCR_RTS;

  if (v & SUART_LINE_DTR)
    x |= PC16550_MCR_DTR;

  if (v & SUART_LINE_OUT0)
    x |= PC16550_MCR_OUT1;

  if (v & SUART_LINE_OUT1)
    x |= PC16550_MCR_OUT2;

  if (v & SUART_LINE_GPIO0)
    x |= PC16550_MCR_LOOP;

  return x;
}

static void suart_pc16x50_change_line (struct suart_pc16x50 * suart,
                                       int mask,
                                       int value)
{
  u8 mcr_mask = suart_line_to_mcr(mask);
  u8 mcr_bits = suart_line_to_mcr(value);
  pc16x50_change_mcr_bits(suart->uart,
                          mcr_bits,
                          mcr_mask);
}



static void flush_rxbuffer_fifo_one_entry(struct suart_pc16x50 * suart)
{
  u8 t[14];
  const int count = kfifo_out(&suart->rxbuffer, t, 14);
  if (count > 0)
    {
      if (debug)
        {
          int i;
          // TODO: send event to rx-event-fifo
          printf ("sendrxevent(%d):", count);
          for (i=0; i < count; ++i)
            printf (" %02X", t[i]);
          printf ("\n");
        }

      struct suart_event_s e;
      int ret = suart_event_data_inline(&e, 1, t, count);
      suart_put_rxevents (&suart->suart, &e, 1);
    }
}

static void flush_rxbuffer_fifo(struct suart_pc16x50 * suart)
{
  while (kfifo_len(&suart->rxbuffer) > 0)
    flush_rxbuffer_fifo_one_entry(suart);
}

static void write_to_rxbuffer_fifo(struct suart_pc16x50 * suart,
                                   u8 c)
{
  if (debug) printf("RX: %02X\n", c);
  kfifo_in(&suart->rxbuffer, &c, 1);
  const int l = kfifo_len(&suart->rxbuffer);
  if (l >= 14)
    {
      flush_rxbuffer_fifo_one_entry(suart);
    }
}

static int is_rxdata_interrupt(u8 isr)
{
  return ((isr & PC16C550_IIR_IRQ_MASK)==PC16C550_IIR_IRQ_RECEIVED_DATA_AVAILABLE)
    ||   ((isr & PC16C550_IIR_IRQ_MASK)==PC16C550_IIR_IRQ_CHARACTER_TIMEOUT)
    ;
}

static void send_line_status_event(struct suart_pc16x50 * suart, u8 linestatus)
{
  struct suart_event_s e;
  e.event_type = STREAMUART_EVENT_LINESTATUS;
  e.changeline.mask = 0xff;
  e.changeline.value = linestatus;
  suart_put_rxevents (&suart->suart, &e, 1);
}

static void handle_all_pending_interrupts(struct suart_pc16x50 * suart)
{
  struct pc16x50 *uart = suart->uart;
  int isr = pc16x50_read_isr(uart);
  while ((isr&1) == 0)
    {
      if (debug) printf ("IRQ ISR:%02X ", isr);
      if (is_rxdata_interrupt(isr))
        {
          write_to_rxbuffer_fifo(suart, pc16x50_read_rbr(suart->uart));

          // If we received a character timeout, we might not know if
          // we get one again, so we flush what we got,
          // even if it is only one octet.
          if ((isr & PC16C550_IIR_IRQ_MASK)==PC16C550_IIR_IRQ_CHARACTER_TIMEOUT)
            flush_rxbuffer_fifo(suart);
        }
      else
        {
          // flush all rx data before handling other causes.
          flush_rxbuffer_fifo(suart);

          switch (isr & PC16C550_IIR_IRQ_MASK)
            {
            case PC16C550_IIR_IRQ_RECEIVER_LINE_STATUS:
              {
                u8 lsr = pc16x50_read_lsr(suart->uart);
                if (lsr & PC16550_LSR_OVERRUN_ERROR)
                    pc16x50_flush_rx_fifo(suart->uart);
                if (lsr & PC16550_LSR_BREAK_INTERRUPT)
                  {
                    // read one dummy byte that was received due to the break.
                    if (pc16x50_read_rbr(suart->uart) != 0)
                      printf ("Ooopps BREAK databyte is not 00\n");
                    // STREAMUART_EVENT_BREAK
                  }
                send_line_status_event(suart, lsr);

#if 1
                if (debug)
                  {
                    printf("RLS: %02X", lsr);
                    if (lsr & PC16550_LSR_DATA_READY)
                      printf (" DR");
                    if (lsr & PC16550_LSR_OVERRUN_ERROR)
                      printf (" OVR");
                    if (lsr & PC16550_LSR_PARITY_ERROR)
                      printf (" PE");
                    if (lsr & PC16550_LSR_FRAMING_ERROR)
                      printf (" FE");
                    if (lsr & PC16550_LSR_BREAK_INTERRUPT)
                      printf (" BI");
                    if (lsr & PC16550_LSR_THRE)
                  printf (" BE");
                    if (lsr & PC16550_LSR_TRANSMITTER_EMPTY)
                      printf (" TEMT");
                    if (lsr & PC16550_LSR_ERROR_IN_RXFIFO)
                      printf (" RFIFOERR");
                    printf ("\n");
                  }
#endif
              }
              break;
              
            case PC16C550_IIR_IRQ_TRANSMITTER_HOLDING_EMPTY:
              if (debug) printf("THRE\n");
              break;

            case PC16C550_IIR_IRQ_MODEM_STATUS:
              if (debug) printf("MODEM\n");
              break;

            case PC16C550_IIR_IRQ_NONE:
              break;

            default:
              if (debug) printf ("???\n");
              break;
            }
        }
      isr = pc16x50_read_isr(uart);
    }
}


static int handle_one_event (struct suart_pc16x50 * suart,
                             struct suart_event_s * e)
{
  struct suart_s *uart = &suart->suart;
  switch (e->event_type)
    {
    case STREAMUART_EVENT_ECHO:
      suart_put_rxevents (uart, e, 1);
      break;

    case STREAMUART_EVENT_SETBAUDRATE:
      {
        const int oversampling = 16;
        const unsigned int divisor = (suart->base_clock / oversampling) / e->baudrate.value;
        pc16x50_set_baudrate_divisor (suart->uart, divisor);
      }
      break;

    case STREAMUART_EVENT_GETBAUDRATE:
      {
        const int divisor = pc16x50_get_baudrate_divisor (suart->uart);
        const int oversampling = 16;
        e->baudrate.value = divisor ? (suart->base_clock / oversampling) / divisor : 0;
        suart_put_rxevents (uart, e, 1);
      }
      break;

    case STREAMUART_EVENT_SETFORMAT:
      pc16x50_set_format (suart->uart,
                          e->format.databits,
                          e->format.parity,
                          e->format.stopbits);
      break;

    case STREAMUART_EVENT_GETFORMAT:
      {
        int  databits;
        char parity;
        int  stopbits;
        pc16x50_get_format (suart->uart,
                            &databits,
                            &parity,
                            &stopbits);
        e->format.databits = databits;
        e->format.parity   = parity;
        e->format.stopbits = stopbits;
        suart_put_rxevents (uart, e, 1);
      }
      break;
      
    case STREAMUART_EVENT_SETHANDSHAKE:
      if (debug) printf ("STREAMUART_EVENT_SETHANDSHAKE\n");
      break;
      
    case STREAMUART_EVENT_GETHANDSHAKE:
      if (debug) printf ("STREAMUART_EVENT_GETHANDSHAKE\n");
      break;
      
    case STREAMUART_EVENT_DATA:
      kfifo_in(&suart->txbuffer, e->data.data, e->data.count);
      return 2; // try to grab as much as possible
      break;
      
    case STREAMUART_EVENT_DATA_INLINE:
      kfifo_in(&suart->txbuffer, e->data_inline.data, e->data_inline.count);
      return 2; // try to grab as much as possible
      break;
      
    case STREAMUART_EVENT_ENABLETX:
      if (debug) printf ("STREAMUART_EVENT_ENABLETX\n");
      break;
      
    case STREAMUART_EVENT_DISABLETX:
      if (debug) printf ("STREAMUART_EVENT_DISABLETX\n");
      break;
      
    case STREAMUART_EVENT_ENABLERX:
      suart->rxenabled = 1;
      pc16x50_flush_rx_fifo(suart->uart);
      pc16x50_set_ier_bits(suart->uart,
                           PC16C550_IER_RECEIVED_DATA_AVAILABLE
                           );
      if (debug) printf ("STREAMUART_EVENT_ENABLERX\n");
      break;
      
    case STREAMUART_EVENT_DISABLERX:
      suart->rxenabled = 0;
      pc16x50_clear_ier_bits(suart->uart,
                             PC16C550_IER_RECEIVED_DATA_AVAILABLE
                             );
      if (debug) printf ("STREAMUART_EVENT_DISABLERX\n");
      break;
      
    case STREAMUART_EVENT_CHANGE_LINE:
      suart_pc16x50_change_line (suart,
                                 e->changeline.mask,
                                 e->changeline.value);
      break;
      
    case STREAMUART_EVENT_ENABLE_NOTIFYCATION:
      if (debug) printf ("STREAMUART_EVENT_ENABLE_NOTIFYCATION\n");
      break;
      
    case STREAMUART_EVENT_DISABLE_NOTIFYCATION:
      if (debug) printf ("STREAMUART_EVENT_DISABLE_NOTIFYCATION\n");
      break;
    }
  return 0;
}

static void wait_until_transmitter_empty(struct suart_pc16x50 * suart)
{
  while ((pc16x50_read_lsr(suart->uart) & PC16550_LSR_TRANSMITTER_EMPTY) == 0)
    usleep(1000);
}

static void fetch_as_much_data_events_as_possible(struct suart_pc16x50 * suart)
{
  struct suart_s *uart = &suart->suart;
  struct suart_event_s e;
  while (suart_peek_txevent (uart, &e) &&
         (e.event_type == STREAMUART_EVENT_DATA ||
          e.event_type == STREAMUART_EVENT_DATA_INLINE))
    {
      if (suart_get_txevents(uart, &e, 1) > 0)
        {
          if (e.event_type == STREAMUART_EVENT_DATA)
            kfifo_in(&suart->txbuffer, e.data.data, e.data.count);
          else if (e.event_type == STREAMUART_EVENT_DATA_INLINE)
            kfifo_in(&suart->txbuffer, e.data_inline.data, e.data_inline.count);
        }
    }
}

static int transmit_as_much_as_possible(struct suart_pc16x50 * suart)
{
  const int l = kfifo_len(&suart->txbuffer);
  if (l > 0)
    {
      const int lsr = pc16x50_read_lsr(suart->uart);
      const int space = (lsr & PC16550_LSR_TRANSMITTER_EMPTY) ? (16+1)
        : (lsr & PC16550_LSR_THRE) ? 16 : 0;
      if (space > 0)
        {
          u8 tmp[17];
          int remain = (space < l) ? space : l;
          remain = kfifo_out(&suart->txbuffer, tmp, remain);
          int i;
          for (i=0; i < remain; ++i)
              pc16x50_write_thr(suart->uart, tmp[i]);
        }
      return l;
    }
  return 0;
}

static int handle_all_pending_events(struct suart_pc16x50 * suart)
{
  struct suart_s *uart = &suart->suart;
  if (kfifo_len(&suart->txbuffer) > 0)
    {
      // When there is data in the txbuffer-fifo, the last event we received are
      // STREAMUART_EVENT_DATA_INLINE or STREAMUART_EVENT_DATA. All the other
      // events execute in nearly 0 time. The data events are the only ones that take
      // more time, depending on the baudrate and the number of octetes to transmit.
      // We first need to send out the serial data before we can execute any other event.
      // Therefor we fetch as much data events as there are until a non-data event is
      // found in the event-fifo.
      // We are later called, because of the THRE interrupt, that causes this function
      // to be called via the handler-thread.
      //
      // What we need to handle are events from the uart like incoming line changes.

      fetch_as_much_data_events_as_possible(suart);
      transmit_as_much_as_possible(suart);
      return 0;
    }

  const int lsr = pc16x50_read_lsr(suart->uart);
  if ((lsr & PC16550_LSR_THRE) == 0)
    return 0; // THRE is not empty, so we get another THRE empty interrupt, hopefully.

  if ((lsr & PC16550_LSR_TRANSMITTER_EMPTY) == 0)
    {
      // THRE is empty, but Transmitter not, wait until it is empty,
      // as we get no Transmitter empty interrupt.
      while ((pc16x50_read_lsr(suart->uart) & PC16550_LSR_TRANSMITTER_EMPTY)==0)
        usleep(1000);
    }

  while (suart_txavailable(uart) > 0)
    {
      struct suart_event_s e;
      if (suart_get_txevents(uart, &e, 1) > 0)
        {
          if (e.event_type == STREAMUART_EVENT_SHUTDOWN)
            return -1;

          const int ret = handle_one_event(suart, &e);
          if (ret == 2)
            {
              // received data event and try to get more
              fetch_as_much_data_events_as_possible(suart);
              transmit_as_much_as_possible(suart);
              return 2;
            }
        }
    }
  return 0;
}

/*
int handle_all_pending_events(struct suart_pc16x50 * suart)
{
  struct suart_s *uart = &suart->suart;
  while (suart_txavailable(uart) > 0)
    {
      struct suart_event_s events[32];
      int ecount = suart_get_txevents(uart, events, sizeof(events)/sizeof(events[0]));
      if (ecount > 0)
        {
          int i;
          for (i = 0; i < ecount; ++i)
            {
              struct suart_event_s * e = &events[i];
              if (e->event_type == STREAMUART_EVENT_SHUTDOWN)
                return -1;
              int ret = handle_one_event(suart, e);
              if (ret)
                return 1;
            }
        }
    }
  return 0;
}
*/

//--------------------------------------------------------

void * suart_handler_pc16x50 (void *arg)
{
  struct suart_pc16x50 * suart = (struct suart_pc16x50 *)arg;
  while (1)
    {
      handle_all_pending_interrupts(suart);
      if (handle_all_pending_events(suart) < 0)
        {
          printf ("shutting down suart_pc16x50\n");
          return 0;
        }

      const int l = kfifo_len(&suart->rxbuffer);
      const int timeout_ms = (l > 0) ? WaitForReceiveDataTimeout_ms : UartIdleTimeout_ms;
      const int ret = wait_event_interruptible_timeout(suart->suart.txwaitqueue, 1, milliseconds_to_jiffies(timeout_ms));
      if (ret == 1)
          flush_rxbuffer_fifo(suart);
    }
}

static void suart_pc16x50_irqfunc(int irq, void *arg)
{
  (void)irq;
  struct suart_pc16x50 * suart = (struct suart_pc16x50 *)arg;
  suart_wakeup (&suart->suart);
}

#ifdef REAL_UART
#else
#include "../uart/wbuart_16x50.h"
#endif

struct suart_pc16x50 * suart_pc16x50_create (wait_queue_head_t *rxwaitqueue)
{
  struct suart_pc16x50 * suart = malloc (sizeof(struct suart_pc16x50));
  if (!suart)
    return 0;

  suart->base_clock = 100000000; // 100MHz

  init_waitqueue_head(&suart->txwaitqueue);
  suart->rxwaitqueue = rxwaitqueue;

  INIT_KFIFO(suart->txbuffer);
  INIT_KFIFO(suart->rxbuffer);

#ifdef REAL_UART
  suart->uart = 0;
#else
  suart->uart = wbuart_pc16x50_create();
#endif
  if (!suart->uart)
    {
      free(suart);
      return 0;
    }

  int ret = suart_init(&suart->suart,
                       &suart->txwaitqueue,
                       suart->rxwaitqueue
                       );

  pthread_create(&suart->suart_handler_thread, NULL, suart_handler_pc16x50, suart);

  suart->rxenabled = 1;
  pc16x50_flush_rx_fifo(suart->uart);
  pc16x50_set_ier_bits(suart->uart,
                       PC16C550_IER_RECEIVED_DATA_AVAILABLE
                       | PC16C550_IER_RECEIVER_LINE_STATUS
                       | PC16C550_IER_MODEM_STATUS
                       | PC16C550_IER_TRANSMITTER_HOLDING_REGISTER_EMPTY
                       );
  pc16x50_set_fifo_trigger_level(suart->uart, 14);

#ifdef REAL_UART
#else
  // last thing to do, after everything is set up, is to assign the irqhandler.
  wbuart_set_irqhandler(suart->uart, suart_pc16x50_irqfunc, suart);
#endif

  return suart;
}

void suart_pc16x50_shutdown(struct suart_pc16x50 * suart)
{
  struct suart_event_s e;
  e.event_type = STREAMUART_EVENT_SHUTDOWN;
  suart_put_txevents (&suart->suart, &e, 1);
  pthread_join(suart->suart_handler_thread, 0);
}


void suart_pc16x50_delete(struct suart_pc16x50 * suart)
{
  if (suart)
    {
      suart_pc16x50_shutdown(suart);
#ifdef REAL_UART
#else
      wbuart_set_irqhandler(suart->uart, 0, 0);
      wbuart_delete(suart->uart);
#endif
      free(suart);
    }
}


////>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

#include "dmx512_suart.h"

struct dmx512suart_port
{
    int dmxstate;
    int dmxsize;
    unsigned char dmx_data[513];
    int dmx_locked;
    int dmx_lock_count;
    int dmx_lock_size;
    struct suart_pc16x50 * suart;
    wait_queue_head_t rxwaitqueue;
    pthread_t rxfifo_handler_thread;
    int       stop_rxhandler;
  
    void (*frame_received)(void *userhandle,
                           unsigned char * dmx_data,
                           int dmxsize);
    void *frame_received_handle;
};


static void dmx512suart_received_frame(struct dmx512suart_port * dmxport,
                                 unsigned char * dmx_data,
                                 int dmxsize)
{
    if (dmxport->frame_received)
        dmxport->frame_received(dmxport->frame_received_handle,
                                dmx_data,
                                dmxsize);
}


static void * dmx512suart_rxfifo_handler (void *arg)
{
  struct dmx512suart_port * dmxport = (struct dmx512suart_port *)arg;
  struct suart_pc16x50 * suart = dmxport->suart;
  struct suart_s *uart = &suart->suart;
  while (!dmxport->stop_rxhandler)
    {
      while (suart_rxavailable(uart) > 0)
        {
          struct suart_event_s e;
          if (suart_get_rxevents(uart, &e, 1) > 0)
            {
              switch (e.event_type)
                {
                case STREAMUART_EVENT_ECHO:
                  printf ("echo reply %u\n", e.echo.eventid);
                  break;

                case STREAMUART_EVENT_GETBAUDRATE:
                  printf ("got baudrate %u\n", e.baudrate.value);
                  break;

                case STREAMUART_EVENT_DATA:
                  printf("rx STREAMUART_EVENT_DATA not implemented\n");
                  exit(1);
                  break;

                case STREAMUART_EVENT_DATA_INLINE:
                  if (dmxport->dmx_locked && (dmxport->dmxstate = 0))
                    {
                      printf ("lost lock. Frame too long\n");
                      dmxport->dmx_lock_count = 0;
                      dmxport->dmx_locked = 0;
                      dmxport->dmx_lock_size = dmxport->dmxsize;
                    }
                  dmxport->dmxstate = 2;
                  if (e.data_inline.count < (512-dmxport->dmxsize))
                    {
                      memcpy (&dmxport->dmx_data[dmxport->dmxsize], e.data_inline.data, e.data_inline.count);
                      dmxport->dmxsize += e.data_inline.count;
                      if(debug) printf ("dmxsize:%d dmx_lock_size:%d  locked:%d\n", dmxport->dmxsize, dmxport->dmx_lock_size, dmxport->dmx_locked);
                      if (dmxport->dmx_locked && (dmxport->dmxsize == dmxport->dmx_lock_size))
                        {
                          dmx512suart_received_frame(dmxport, dmxport->dmx_data, dmxport->dmxsize);
                          dmxport->dmxsize = 0;
                          dmxport->dmxstate = 0;
                        }
                    }
                  if(debug) hexdump("rx STREAMUART_EVENT_DATA_INLINE:", e.data_inline.data, e.data_inline.count, "\n");
                  break;

                case STREAMUART_EVENT_LINESTATUS:
                  if (e.changeline.value & PC16550_LSR_BREAK_INTERRUPT)
                    {
                      if (dmxport->dmxsize > 0)
                        {
                          if (dmxport->dmx_locked)
                            {
                              if (dmxport->dmx_lock_size != dmxport->dmxsize)
                                {
                                  printf ("lost lock. Frame too short\n");
                                  dmxport->dmx_lock_count = 0;
                                  dmxport->dmx_locked = 0;
                                  dmxport->dmx_lock_size = dmxport->dmxsize;
                                }
                            }
                          else
                            {
                              if (dmxport->dmx_lock_size == dmxport->dmxsize)
                                dmxport->dmx_lock_count++;
                              if (dmxport->dmx_lock_count > 3)
                                {
                                  printf ("locked to %d\n", dmxport->dmx_lock_size);
                                  dmxport->dmx_locked = 1;
                                }
                              dmxport->dmx_lock_size = dmxport->dmxsize;
                            }

                          dmx512suart_received_frame(dmxport, dmxport->dmx_data, dmxport->dmxsize);
                          dmxport->dmxsize=0;
                        }
                      dmxport->dmxstate = 1;
                      dmxport->dmxsize = 0;
                      if(debug) printf ("Break detected\n");
                    }
                  break;

                case STREAMUART_EVENT_SETBAUDRATE:          if(debug) printf("rx STREAMUART_EVENT_SETBAUDRATE:\n"); break;
                case STREAMUART_EVENT_SETFORMAT:            if(debug) printf("rx STREAMUART_EVENT_SETFORMAT:\n"); break;
                case STREAMUART_EVENT_GETFORMAT:            if(debug) printf("rx STREAMUART_EVENT_GETFORMAT:\n"); break;
                case STREAMUART_EVENT_SETHANDSHAKE:         if(debug) printf("rx STREAMUART_EVENT_SETHANDSHAKE:\n"); break;
                case STREAMUART_EVENT_GETHANDSHAKE:         if(debug) printf("rx STREAMUART_EVENT_GETHANDSHAKE:\n"); break;
                case STREAMUART_EVENT_ENABLETX:             if(debug) printf("rx STREAMUART_EVENT_ENABLETX:\n"); break;
                case STREAMUART_EVENT_DISABLETX:            if(debug) printf("rx STREAMUART_EVENT_DISABLETX:\n"); break;
                case STREAMUART_EVENT_ENABLERX:             if(debug) printf("rx STREAMUART_EVENT_ENABLERX:\n"); break;
                case STREAMUART_EVENT_DISABLERX:            if(debug) printf("rx STREAMUART_EVENT_DISABLERX:\n"); break;
                case STREAMUART_EVENT_CHANGE_LINE:          if(debug) printf("rx STREAMUART_EVENT_CHANGE_LINE:\n"); break;
                case STREAMUART_EVENT_ENABLE_NOTIFYCATION:  if(debug) printf("rx STREAMUART_EVENT_ENABLE_NOTIFYCATION:\n"); break;
                case STREAMUART_EVENT_DISABLE_NOTIFYCATION: if(debug) printf("rx STREAMUART_EVENT_DISABLE_NOTIFYCATION:\n"); break;

                default:
                  if(debug) printf ("received...\n");
                  break;
                }
            }
        }
      const int ret = wait_event_interruptible_timeout(suart_rxwaitqueue (&suart->suart), 1, milliseconds_to_jiffies(2000));
    }
  printf ("stopping\n");
  return 0;
}


static  int dmx512suart_create_frame(struct suart_event_s * events,
                                    const int maxevents,
                                    int startcode,
                                    unsigned char *dmxdata,
                                    int slotcount)
{
  int count = 0;
  int ret;

  if ((13 + (slotcount+13)/14) > maxevents)
    return -1;
  
  suart_event_disable_rx(&events[count++]);
  suart_event_enable_tx(&events[count++]);
  suart_event_change_line(&events[count++], SUART_LINE_RTS, SUART_LINE_RTS);
  suart_event_set_format(&events[count++], 8, 'n', 1);
  suart_event_change_baudrate(&events[count++], 100500);
  suart_event_data_inline_small_va(&events[count++], 1, 0);
  suart_event_set_format(&events[count++], 8, 'n', 2);
  suart_event_change_baudrate(&events[count++], 250000);
  suart_event_data_inline_small_va(&events[count++], 1, 0);
  ret = suart_event_data_inline(&events[count], maxevents-13, dmxdata, slotcount);
  if (ret <= 0)
    return -1;
  count += ret;
  suart_event_change_line(&events[count++], SUART_LINE_RTS, 0);
  suart_event_disable_tx(&events[count++]);
  suart_event_enable_rx(&events[count++]);
  suart_event_echo(&events[count++], 1);
  return count;
}

int dmx512suart_send_frame(struct dmx512suart_port * dmxport,
                           unsigned char *dmxdata,
                           int count)
{
    if (dmxport && dmxport->suart)
    {
        struct suart_event_s events[128];
        const int n = dmx512suart_create_frame(events, 128, dmxdata[0], &dmxdata[1], count-1);
        suart_put_txevents (&dmxport->suart->suart, events, n);
    }
}


static void dmx512suart_send_init_events(struct dmx512suart_port * dmxport)
{
    int count = 0;
    struct suart_event_s events[6];
    suart_event_set_format(&events[count++], 8, 'n', 2);
    suart_event_change_baudrate(&events[count++], 250000);
    suart_event_disable_tx(&events[count++]);
    suart_event_enable_rx(&events[count++]);
    suart_event_change_line(&events[count++], SUART_LINE_RTS | SUART_LINE_DTR, SUART_LINE_DTR);
    suart_event_echo(&events[count++], 12345678);
    if (dmxport && dmxport->suart)
        suart_put_txevents (&dmxport->suart->suart, events, count);
}

void dmx512suart_set_dtr (struct dmx512suart_port * port, int value)
{
    if (port && port->suart)
    {
        struct suart_event_s event;
        suart_event_change_line(&event, SUART_LINE_DTR, value ? SUART_LINE_DTR : 0);
        suart_put_txevents (&port->suart->suart, &event, 1);
    }
}


void dmx512suart_port_init(struct dmx512suart_port * dmxport,
                           struct suart_pc16x50 * suart,
                           void (*frame_received)(void *userhandle,
                                                  unsigned char * dmx_data,
                                                  int dmxsize),
                           void * frame_received_handle
                           )
{
  dmxport->suart = suart;
  dmxport->dmxstate = 0;
  dmxport->dmxsize = 0;
  dmxport->dmx_locked = 0;
  dmxport->dmx_lock_count = 0;
  dmxport->dmx_lock_size = 0;
  dmxport->frame_received = frame_received;
  dmxport->frame_received_handle = frame_received_handle;
}


struct dmx512suart_port * dmx512suart_create_port (
    void (*frame_received)(void *userhandle,
                           unsigned char * dmx_data,
                           int dmxsize),
    void * frame_received_handle
    )
{
    struct dmx512suart_port * port = (struct dmx512suart_port *)malloc(sizeof(*port));
    if (port)
    {
        init_waitqueue_head(&port->rxwaitqueue);
        struct suart_pc16x50 * suart = suart_pc16x50_create (&port->rxwaitqueue);
        if (!suart)
        {
            printf ("failed to create suart_pc16x50\n");
            free(port);
            return 0;
        }

        //= Create DMX receiver ====
        dmx512suart_port_init (port, suart, frame_received, frame_received_handle);
        port->stop_rxhandler = 0;
        pthread_create(&port->rxfifo_handler_thread, NULL, dmx512suart_rxfifo_handler, port);

        dmx512suart_send_init_events(port);
    }
    return port;
}

void dmx512suart_delete_port (struct dmx512suart_port * port)
{
    if (port)
    {
        if (!port->stop_rxhandler)
        {
            port->stop_rxhandler = 1;
            wake_up_interruptible(suart_rxwaitqueue (&port->suart->suart));
            pthread_join(port->rxfifo_handler_thread, 0);
        }
        if (port->suart)
        {
            suart_pc16x50_delete(port->suart);  // does a suart_pc16x50_shutdown(suart);
            port->suart = 0;
        }
        free(port);
    }
}


//===================== testing code ==============
#if 0
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
#endif
