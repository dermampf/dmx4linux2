#ifndef DEFINED_SUART_EVENT_H
#define DEFINED_SUART_EVENT_H

#include <linux/types.h>

/*
 * Sets the baudrate.
 */

typedef enum
  {
    STREAMUART_EVENT_SHUTDOWN = 255,
    STREAMUART_EVENT_ECHO = 1,

    STREAMUART_EVENT_SETBAUDRATE,
    STREAMUART_EVENT_GETBAUDRATE,
    STREAMUART_EVENT_SETFORMAT,
    STREAMUART_EVENT_GETFORMAT,
    STREAMUART_EVENT_SETHANDSHAKE,
    STREAMUART_EVENT_GETHANDSHAKE,

    STREAMUART_EVENT_DATA,
    STREAMUART_EVENT_DATA_INLINE,

    STREAMUART_EVENT_ENABLETX, // STREAMUART_EVENT_CHANGE_ENABLE with mask and value or values ENABLETX, DISABLETX, ENABLERX, DISABLERX
    STREAMUART_EVENT_DISABLETX,
    STREAMUART_EVENT_ENABLERX,
    STREAMUART_EVENT_DISABLERX,

    STREAMUART_EVENT_CHANGE_LINE,

    STREAMUART_EVENT_ENABLE_NOTIFYCATION, // => STREAMUART_EVENT_CHANGE_NOTIFYCATION with mask and value
    STREAMUART_EVENT_DISABLE_NOTIFYCATION,

    STREAMUART_EVENT_LINESTATUS,
  } suart_event_type;


typedef enum {
  RTUART_HANDSHAKE_OFF   = 0,
  RTUART_HANDSHAKE_RTSCTS = 1,
  RTUART_HANDSHAKE_DSRDTR = 2,
  RTUART_HANDSHAKE_RTSCTS_DSRDTR = 3
} suart_handshake_t;

typedef enum {
  SUART_LINE_RTS = 1<<0,
  SUART_LINE_CTS = 1<<1,
  SUART_LINE_DTR = 1<<2,
  SUART_LINE_DSR = 1<<3,
  SUART_LINE_DCD = 1<<4,
  SUART_LINE_RI = 1<<5,
  SUART_LINE_OUT0 = 1<<6,
  SUART_LINE_OUT1 = 1<<7,

  SUART_LINE_GPIO0 = 1<<8,
  SUART_LINE_GPIO1 = 1<<9,
  SUART_LINE_GPIO2 = 1<<10,
  SUART_LINE_GPIO3 = 1<<11,
  SUART_LINE_GPIO4 = 1<<12,
  SUART_LINE_GPIO5 = 1<<13,
  SUART_LINE_GPIO6 = 1<<14,
  SUART_LINE_GPIO7 = 1<<15
} suart_line;

enum { SUART_OCTETS_PER_INLINE_DATA_CHUNK = 14 };

struct suart_event_s
{
  union {
    //__u32             event_size;  // size of this event // sizeof(struct suart_event)
  //suart_event_type  event_type;
    __u8 event_type;

    struct {
      __u8 __data_t;
      __u8 __unused[5];
      __u16  count; // 2 byte
      __u8  *data; // 8 byte
    } data;

    struct {
      __u8 __data_inline_t;
      __u8 count;
      __u8 data[SUART_OCTETS_PER_INLINE_DATA_CHUNK];
    } data_inline;

    struct {
      __u8 __baudrate_t;
      __u8 __unused[3];
      __u32 value;
    } baudrate;

    struct {
      __u8 __format_t;
      __u8 databits;
      char parity;
      __u8 stopbits;
    } format;

    struct {
      __u8 __handshake_t;
      suart_handshake_t handshake;
    };

    struct {
      __u8 __linemask_t;
      __u32 mask;
      __u32 value;
    } changeline;

    struct {
      __u8 __eventid_t;
      __u32 eventid;
    } echo;
  };
};
typedef struct suart_event_s suart_event_t;


void suart_event_set_format(struct suart_event_s *e,
                            int databits,
                            char parity,
                            int stopbits);
void suart_event_enable_tx  (struct suart_event_s *e);
void suart_event_disable_tx (struct suart_event_s *e);
void suart_event_enable_rx  (struct suart_event_s *e);
void suart_event_disable_rx (struct suart_event_s *e);
void suart_event_change_line(struct suart_event_s *e, int mask, int value);
void suart_event_change_baudrate (struct suart_event_s *e, const unsigned long baudrate);
void suart_event_query_baudrate (struct suart_event_s *e);
void suart_event_echo (struct suart_event_s *e, int value);
int  suart_event_data_inline_small_va (struct suart_event_s *e, const int count, ...);
int  suart_event_data_inline_small (struct suart_event_s *e, const void *data, int count);

/*
 * Split a buffer into multiple events and return how much event have been used.
 * @e a vector of events
 * @maxevents the maximum number of events tha can be used
 * @data  points to the octet buffer
 * @count number of octets to pack into the events.
 */
int suart_event_data_inline (struct suart_event_s *e, const int maxevents, const void *data, int count);

#endif
