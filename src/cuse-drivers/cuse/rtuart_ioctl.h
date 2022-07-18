#pragma once

// _IOR | _IOW | _IORW

#include <rtuart_values.h>

enum {
	RTUART_SET_FORMAT = 1,
	RTUART_GET_FORMAT,

	RTUART_SET_BAUDRATE,
	RTUART_GET_BAUDRATE,

	RTUART_SET_CONTROL, /* break, rts, dtr, loop, ... */
	RTUART_CLR_CONTROL, /* break, rts, dtr, loop, ... */
	RTUART_GET_CONTROL,

	RTUART_SET_NOTIFY, /* break, rts, dtr, loop, ... */
	RTUART_CLR_NOTIFY, /* break, rts, dtr, loop, ... */
	RTUART_GET_NOTIFY,

	RTUART_TX_ENABLE,
	RTUART_RX_ENABLE,
};

#define RTUART_IOCTL_S_FMT       _IOW('Z', RTUART_SET_FORMAT, unsigned long)
#define RTUART_IOCTL_G_FMT       _IOR('Z', RTUART_GET_FORMAT, unsigned long)

#define RTUART_IOCTL_S_BAUDRATE  _IOW('Z', RTUART_SET_BAUDRATE, unsigned long)
#define RTUART_IOCTL_G_BAUDRATE  _IOR('Z', RTUART_GET_BAUDRATE, unsigned long)

#define RTUART_IOCTL_S_CONTROL   _IOW('Z', RTUART_SET_CONTROL, unsigned long)
#define RTUART_IOCTL_C_CONTROL   _IOW('Z', RTUART_CLR_CONTROL, unsigned long)
#define RTUART_IOCTL_G_CONTROL   _IOR('Z', RTUART_GET_CONTROL, unsigned long)

#define RTUART_IOCTL_S_NOTIFY   _IOW('Z', RTUART_SET_NOTIFY, unsigned long)
#define RTUART_IOCTL_C_NOTIFY   _IOW('Z', RTUART_CLR_NOTIFY, unsigned long)
#define RTUART_IOCTL_G_NOTIFY   _IOR('Z', RTUART_GET_NOTIFY, unsigned long)

#define RTUART_IOCTL_TX_ENABLE   _IOW('Z', RTUART_TX_ENABLE, unsigned long)
#define RTUART_IOCTL_RX_ENABLE   _IOW('Z', RTUART_RX_ENABLE, unsigned long)

enum {
  RTUART_WHAT_CALLBACK_TX_START,
  RTUART_WHAT_CALLBACK_TX_END1,
  RTUART_WHAT_CALLBACK_TX_END2,
  RTUART_WHAT_CALLBACK_INPUT_CHANGE,
  RTUART_WHAT_CALLBACK_RX_ERR,
  RTUART_WHAT_CALLBACK_RX_CHAR,
  RTUART_WHAT_CALLBACK_RX_END,
  RTUART_WHAT_CALLBACK_RX_TIMEOUT,
  RTUART_WHAT_CALLBACK_RX_TRIGGER,
};

struct rtuart_read_event {
  unsigned char what;
  union {
    struct {
      unsigned short buffer_id;
    } tx_start;
    struct {
      unsigned short buffer_id;
    } tx_end1;
    struct {
      unsigned short buffer_id;
    } tx_end2;
    struct {
      unsigned long error_mask;
    } rx_err;
    struct {
      unsigned long eventmask;
      unsigned long values;
    } input_change;
    struct {
      unsigned short c;
    } rx_char;
    struct {
      unsigned short buffer_id;
    } rx_end;
    struct {
      unsigned short buffer_id;
    } rx_timeout;
    struct {
      unsigned short buffer_id;
      unsigned long  trigger_mask;
    } rx_trigger;
  };
};

/*
 * struct rtuart_read_event e;
 * read(fd, &r, sizeof(e));
 */
