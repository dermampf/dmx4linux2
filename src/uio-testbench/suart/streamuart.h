#ifndef DEFINED_SUART_H
#define DEFINED_SUART_H

/*
 * Sets the baudrate.
 */

typedef enum
  {
    STREAMUART_EVENT_ECHO,
    STREAMUART_EVENT_SETBAUDRATE,
    STREAMUART_EVENT_GETBAUDRATE,
    STREAMUART_EVENT_SETFORMAT,
    STREAMUART_EVENT_GETFORMAT,
    STREAMUART_EVENT_SETHANDSHAKE,
    STREAMUART_EVENT_GETHANDSHAKE,
    STREAMUART_EVENT_DATA,
    STREAMUART_EVENT_DATA_INLINE,
    STREAMUART_EVENT_ENABLETX,
    STREAMUART_EVENT_DISABLETX,
    STREAMUART_EVENT_ENABLERX,
    STREAMUART_EVENT_DISABLERX,
    STREAMUART_EVENT_SET_LINE,
    STREAMUART_EVENT_CLR_LINE,
    STREAMUART_EVENT_ENABLE_NOTIFYCATION,
    STREAMUART_EVENT_DISABLE_NOTIFYCATION,
  } suart_event_type;


typedef enum {
  RTUART_HANDSHAKE_OFF   = 0,
  RTUART_HANDSHAKE_RTSCTS = 1,
  RTUART_HANDSHAKE_DSRDTR = 2,
  RTUART_HANDSHAKE_RTSCTS_DSRDTR = 3
} suart_handshake_t;

typedef enum {
  SUART_LINE_RTS,
  SUART_LINE_CTS,
  SUART_LINE_DTR,
  SUART_LINE_DSR,
  SUART_LINE_DCD,
  SUART_LINE_RI,
  SUART_LINE_OUT0,
  SUART_LINE_OUT1
} suart_line;


struct suart_event
{
  uint32_t          event_size;  // size of this event // sizeof(struct suart_event)
  suart_event_type  event_type;
  union {
    struct {
      uint32_t count;
      uint8_t *data;
    } data;
    struct {
      uint8_t count;
      uint8_t data[4+3];
    } data_inline;
    unsigned long baudrate;
    struct {
      uint8_t databits;
      char    parity;
      uint8_t stopbits;
    } format;
    suart_handshake_t handshake;
    uint32_t linemask;
    uint32_t eventid;
  } arguments;
};

#endif
