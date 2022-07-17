#include "streamuart.h"
#include "streamuart_event.h"
#include "kfifo.h"
#include <pthread.h>
#include <stdio.h>


int handle_all_pending_events(struct suart_s *uart)
{
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
              switch (e->event_type)
                {
                case STREAMUART_EVENT_SHUTDOWN:
                  return -1;

                case STREAMUART_EVENT_ECHO:
                  printf ("STREAMUART_EVENT_ECHO\n");
                  break;
                case STREAMUART_EVENT_SETBAUDRATE:
                  printf ("STREAMUART_EVENT_SETBAUDRATE: %u\n",
                          e->baudrate.value);
                  break;
                case STREAMUART_EVENT_GETBAUDRATE:
                  printf ("STREAMUART_EVENT_GETBAUDRATE\n");
                  break;
                case STREAMUART_EVENT_SETFORMAT:
                  printf ("STREAMUART_EVENT_SETFORMAT: %d%c%d\n",
                          e->format.databits,
                          e->format.parity,
                          e->format.stopbits
                          );
                  break;
                case STREAMUART_EVENT_GETFORMAT:
                  printf ("STREAMUART_EVENT_GETFORMAT\n");
                  break;
                case STREAMUART_EVENT_SETHANDSHAKE:
                  printf ("STREAMUART_EVENT_SETHANDSHAKE\n");
                  break;
                case STREAMUART_EVENT_GETHANDSHAKE:
                  printf ("STREAMUART_EVENT_GETHANDSHAKE\n");
                  break;
                case STREAMUART_EVENT_DATA:
                  printf ("STREAMUART_EVENT_DATA\n");
                  for (i=0; i < e->data.count; ++i)
                    printf (" %02X ", e->data.data[i]);
                  printf ("\n");
                  break;
                case STREAMUART_EVENT_DATA_INLINE:
                  printf ("STREAMUART_EVENT_DATA_INLINE:");
                  {
                    int i;
                    for (i=0; i < e->data_inline.count; ++i)
                      printf (" %02X ", e->data_inline.data[i]);
                    printf ("\n");
                  }
                  break;
                case STREAMUART_EVENT_ENABLETX:
                  printf ("STREAMUART_EVENT_ENABLETX\n");
                  break;
                case STREAMUART_EVENT_DISABLETX:
                  printf ("STREAMUART_EVENT_DISABLETX\n");
                  break;
                case STREAMUART_EVENT_ENABLERX:
                  printf ("STREAMUART_EVENT_ENABLERX\n");
                  break;
                case STREAMUART_EVENT_DISABLERX:
                  printf ("STREAMUART_EVENT_DISABLERX\n");
                  break;
                case STREAMUART_EVENT_CHANGE_LINE:
                  printf ("STREAMUART_EVENT_CHANGE_LINE: mask:%u value:%u\n",
                          e->changeline.mask,
                          e->changeline.value
                          );
                  break;
                case STREAMUART_EVENT_ENABLE_NOTIFYCATION:
                  printf ("STREAMUART_EVENT_ENABLE_NOTIFYCATION\n");
                  break;
                case STREAMUART_EVENT_DISABLE_NOTIFYCATION:
                  printf ("STREAMUART_EVENT_DISABLE_NOTIFYCATION\n");
                  break;
                }
            }
        }
    }
  return 0;
}

void * suart_handler (void *arg)
{
  struct suart_s *uart = (struct suart_s *)arg;
  while (1)
    {
      if (handle_all_pending_events(uart) < 0)
        {
          printf ("shutting down suart\n");
          return 0;
        }
      const int ret = wait_event_interruptible_timeout(suart_txwaitqueue(uart), 1, milliseconds_to_jiffies(2000));
    }
  return 0;
}

#include "streamuart_struct.h"

int main ()
{
  wait_queue_head_t  txwaitqueue;
  wait_queue_head_t  rxwaitqueue;
  init_waitqueue_head(&txwaitqueue);
  init_waitqueue_head(&rxwaitqueue);

  struct suart_s     suart;
  int ret = suart_init(&suart,
                       &txwaitqueue,
                       &rxwaitqueue
                       );

  pthread_t          suart_handler_thread;
  pthread_create(&suart_handler_thread, NULL, suart_handler, &suart);



  //sleep(1);

  //======= initalize uart =====
  {
    int count = 0;
    struct suart_event_s events[5];
    suart_event_set_format(&events[count++], 8, 'n', 1);
    suart_event_disable_tx(&events[count++]);
    suart_event_enable_rx(&events[count++]);
    suart_event_change_line(&events[count++], SUART_LINE_RTS, 0);
    suart_event_echo(&events[count++], 12345678);
    suart_put_txevents (&suart, events, count);
  }

  sleep(2);

  //==== encode dmx-frame ====
  struct suart_event_s events[128];
  int count = 0;
  suart_event_disable_rx(&events[count++]);
  suart_event_enable_tx(&events[count++]);
  suart_event_change_line(&events[count++], SUART_LINE_RTS, SUART_LINE_RTS);
  suart_event_change_baudrate(&events[count++], 250000/3);
  suart_event_data_inline_small_va(&events[count++], 1, 0);
  suart_event_change_baudrate(&events[count++], 250000);
  suart_event_data_inline_small_va(&events[count++], 1, 0);
  {
    unsigned char dmxdata[512];
    int i;
    for (i=0; i<512; ++i)
      dmxdata[i] = 1+i;
    int ret = suart_event_data_inline(&events[count], 64-count-5, dmxdata, 512);
    printf ("%d\n", ret);
    if (ret > 0)
      count += ret;
  }
  suart_event_change_line(&events[count++], SUART_LINE_RTS, 0);
  suart_event_disable_tx(&events[count++]);
  suart_event_enable_rx(&events[count++]);
  suart_event_echo(&events[count++], 1);
  suart_put_txevents (&suart, events, count);


  //==============================
  // handle_all_pending_events(&uart);

  sleep(1);



  // Shutdown suart process
  {
    struct suart_event_s e;
    e.event_type = STREAMUART_EVENT_SHUTDOWN;
    suart_put_txevents (&suart, &e, 1);
    pthread_join(suart_handler_thread, 0);
  }

  return 0;
}




