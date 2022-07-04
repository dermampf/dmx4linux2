// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#ifndef DEFINED_RTUART
#define DEFINED_RTUART

#include "kernel.h"

#include <stdio.h>

struct rtuart_bus;
struct rtuart_client;
struct rtuart
{
  struct rtuart_bus * bus;
  struct rtuart_ops * ops;
  struct rtuart_client * client;
  u32 notify_mask;
};

enum rtuart_notify
{
	UART_NOTIFY_TXEMPTY = 0x01,
	UART_NOTIFY_TXREADY = 0x02,
	UART_NOTIFY_RXAVAILABLE = 0x04,
	UART_NOTIFY_INPUTCHANGED = 0x08,
	UART_NOTIFY_LINEEVENT = 0x10
};

enum rtuart_event
{
	RTUART_EVENT_OVERRUN_ERROR = (1<<0),
	RTUART_EVENT_PARITY_ERROR  = (1<<1),
	RTUART_EVENT_FRAMING_ERROR = (1<<2),
	RTUART_EVENT_BREAK         = (1<<3)
};

enum {
  RTUART_OUTPUT_DTR = (1<<0),
  RTUART_OUTPUT_RTS = (1<<1),
  RTUART_OUTPUT_GENERIC0 = (1<<2),
  RTUART_OUTPUT_GENERIC1 = (1<<3)
};


#include "rtuart_bus.h"

static inline int rtuart_read_u8(struct rtuart * u, const int reg, u8 * value)
{
  return rtuart_bus_read_u8(u->bus, reg, value);
}

static inline int rtuart_read_u16(struct rtuart * u, const int reg, u16 * value)
{
  return rtuart_bus_read_u16(u->bus, reg, value);
}

static inline int rtuart_read_u32(struct rtuart * u, const int reg, u32 * value)
{
  return rtuart_bus_read_u32(u->bus, reg, value);
}

static inline int rtuart_write_u8(struct rtuart * u, const int reg, const u8 value)
{
  return rtuart_bus_write_u8(u->bus, reg, value);
}

static inline int rtuart_write_u16(struct rtuart * u, const int reg, const u16 value)
{
  return rtuart_bus_write_u16(u->bus, reg, value);
}

static inline int rtuart_write_u32(struct rtuart * u, const int reg, const u32 value)
{
  return rtuart_bus_write_u32(u->bus, reg, value);
}


#include "rtuart_ops.h"

/* parity: 'n','e','o', '1', '0'. stopbits: 1=10, 1,5=15, 2=20 */
static inline int rtuart_set_format(struct rtuart * u,
				    const int databits,
				    const char parity,
				    const int stopbits)
{
  printf ("rtuart_set_format: u:%p u->ops:%p  u->ops->set_format:%p\n",
	  u,
	  u ? u->ops : (void*)-1,
	  (u && u->ops) ? u->ops->set_format : (void*)-1);
  return u->ops->set_format(u, databits, parity, stopbits);
}

static inline int rtuart_get_format(struct rtuart *u,
				    int * databits,
				    char * parity,
				    int * stopbits)
{
  return u->ops->get_format(u, databits, parity, stopbits);
}

static inline int rtuart_set_baudrate(struct rtuart *u,
				      const unsigned int baudrate)
{
  return u->ops->set_baudrate(u, baudrate);
}

static inline int rtuart_get_baudrate(struct rtuart *u,
				      unsigned int * baudrate)
{
  return u->ops->get_baudrate(u, baudrate);
}

static inline int rtuart_set_break(struct rtuart *u,
				   const int on)
{
  return u->ops->set_break(u, on);
}

static inline int rtuart_get_break(struct rtuart *u,
				   int *on)
{
  return u->ops->get_break(u, on);
}

static inline int rtuart_get_line_status(struct rtuart *u,
					 unsigned int * line)
{
  return u->ops->get_line_status(u, line);
}

// see RTUART_OUTPUT_*
static inline int rtuart_set_modem_lines(struct rtuart *u,
					 unsigned int mask,
					 unsigned int line)
{
  return u->ops->set_modem_lines(u, mask, line);
}

static inline int rtuart_set_fifo_enable(struct rtuart *u,
					 const int on)
{
  return u->ops->set_fifo_enable(u, on);
}

static inline int rtuart_flush_fifo (struct rtuart *u,
				     const int flush_tx,
				     const int flush_rx)
{
  return u->ops->flush_fifo (u, flush_tx, flush_rx);
}

static inline int rtuart_set_rxfifo_trigger_level (struct rtuart *u,
						   const int level)
{
  return u->ops->set_rxfifo_trigger_level (u, level);
}

static inline int rtuart_set_txfifo_trigger_level (struct rtuart *u,
						   const int level)
{
  return (u->ops->set_txfifo_trigger_level) ? u->ops->set_txfifo_trigger_level (u, level) : -1;
}


// returns 1 if there is a THR and 0 if there is only the shift register.
static inline int rtuart_get_tx_fifo_size(struct rtuart *u,
					  unsigned int * level)
{
  return u->ops->get_tx_fifo_size(u, level);
}

static inline int rtuart_get_rx_fifo_size(struct rtuart *u,
					  unsigned int * level)
{
  return u->ops->get_rx_fifo_size(u, level);
}

// how many free space is in the tx fifo.
static inline int rtuart_get_tx_fifo_free(struct rtuart *u,
					  unsigned int * level)
{
  return u->ops->get_tx_fifo_free(u, level);
}

// how many octets are available in the rx fifo.
static inline int rtuart_get_rx_fifo_level(struct rtuart *u,
					   unsigned int * level)
{
  return u->ops->get_rx_fifo_level(u, level);
}

static inline int rtuart_write_chars (struct rtuart *u,
				      const u8 * buffer,
				      const size_t count)
{
  return u->ops->write_chars (u, buffer, count);
}

static inline int rtuart_read_chars (struct rtuart *u,
				     u8 * buffer,
				     const size_t count)
{
  return u->ops->read_chars (u, buffer, count);
}

static inline int rtuart_handle_irq(struct rtuart *u)
{
  return u->ops->handle_irq(u);
}

/* Notification will be automatically disabled once the notifier function is called. */
static inline int uart_enable_notification(struct rtuart * u , enum rtuart_notify n)
{
	return u->ops->enable_notification(u, n);
}

static inline int uart_disable_notification(struct rtuart * u , enum rtuart_notify n)
{
	return u->ops->disable_notification(u, n);
}

#endif
