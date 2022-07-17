#include "streamuart_event.h"
#include <stdarg.h>

void suart_event_set_format(struct suart_event_s *e,
                            int databits,
                            char parity,
                            int stopbits)
{
  e->event_type = STREAMUART_EVENT_SETFORMAT;
  e->format.databits = databits;
  e->format.parity   = parity;
  e->format.stopbits = stopbits;
}

void suart_event_enable_tx(struct suart_event_s *e)
{
  e->event_type = STREAMUART_EVENT_ENABLETX;
}

void suart_event_disable_tx(struct suart_event_s *e)
{
  e->event_type = STREAMUART_EVENT_DISABLETX;
}

void suart_event_enable_rx(struct suart_event_s *e)
{
  e->event_type = STREAMUART_EVENT_ENABLERX;
}

void suart_event_disable_rx(struct suart_event_s *e)
{
  e->event_type = STREAMUART_EVENT_DISABLERX;
}

void suart_event_change_line(struct suart_event_s *e, int mask, int value)
{
  e->event_type = STREAMUART_EVENT_CHANGE_LINE;
  e->changeline.mask  = mask;
  e->changeline.value = value;
}

void suart_event_change_baudrate(struct suart_event_s *e, const unsigned long baudrate)
{
  e->event_type = STREAMUART_EVENT_SETBAUDRATE;
  e->baudrate.value = baudrate;
}

void suart_event_query_baudrate(struct suart_event_s *e)
{
  e->event_type = STREAMUART_EVENT_GETBAUDRATE;
}

void suart_event_echo(struct suart_event_s *e, int value)
{
  e->event_type = STREAMUART_EVENT_ECHO;
  e->echo.eventid = value;
}

#include <stdarg.h>
int suart_event_data_inline_small_va(struct suart_event_s *e, const int count, ...)
{
  va_list ap;
  int i;

  va_start(ap, count);

  if (count > SUART_OCTETS_PER_INLINE_DATA_CHUNK)
    return -1;

  e[0].event_type = STREAMUART_EVENT_DATA_INLINE;
  e[0].data_inline.count = count;
  for (i = 0; i < count; ++i)
    e[0].data_inline.data[i] = (int)va_arg(ap, int);
  va_end(ap);
  return 1;
}

int suart_event_data_inline_small(struct suart_event_s *e, const void *data, int count)
{
  int i;
  if (count > SUART_OCTETS_PER_INLINE_DATA_CHUNK)
    return -1;
  e[0].event_type = STREAMUART_EVENT_DATA_INLINE;
  e[0].data_inline.count = count;
  for (i = 0; i < count; ++i)
    e[0].data_inline.data[i] = ((unsigned char*)data)[i];
  return 1;
}

/*
 * Split it up into multiple events.
 */
int suart_event_data_inline(struct suart_event_s *e, const int maxevents, const void *data, int count)
{
  unsigned char * pdata = (unsigned char *)data;
  int eventindex;
  int remain;

  if ((count+6) / SUART_OCTETS_PER_INLINE_DATA_CHUNK > maxevents)
      return -1;

  for (eventindex = 0, remain = count;
       remain > 0;
       eventindex++)
  {
      int i;
      const int chunksize = (remain > SUART_OCTETS_PER_INLINE_DATA_CHUNK) ? SUART_OCTETS_PER_INLINE_DATA_CHUNK : remain;
      e[eventindex].event_type = STREAMUART_EVENT_DATA_INLINE;
      e[eventindex].data_inline.count = chunksize;
      for (i = 0; i < chunksize; ++i)
        e[eventindex].data_inline.data[i] = *(pdata++);
      remain -= chunksize;
  }
  return eventindex;
}
