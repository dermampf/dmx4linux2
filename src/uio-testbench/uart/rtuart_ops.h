// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#ifndef DEFINED_RTUART_OPS
#define DEFINED_RTUART_OPS

#include "kernel.h"

struct rtuart;
struct rtuart_ops
{
	/* parity: 'n','e','o', '1', '0'. stopbits: 1=10, 1,5=15, 2=20 */
	int (*set_format)(struct rtuart *, const int databits, const char parity, const int stopbits);
	int (*get_format)(struct rtuart *, int * databits, char * parity, int * stopbits);

	int (*set_baudrate)(struct rtuart *, const unsigned int baudrate);
	int (*get_baudrate)(struct rtuart *, unsigned int * baudrate);

	int (*set_break)(struct rtuart *, const int on);
	int (*get_break)(struct rtuart *, int *on);

	int (*get_line_status)(struct rtuart *, unsigned int * line);
	int (*set_modem_lines)(struct rtuart *,unsigned int mask, unsigned int line);

	int (*set_fifo_enable)(struct rtuart *, const int on);
	int (*flush_fifo) (struct rtuart *, const int flush_tx, const int flush_rx);
	int (*set_rxfifo_trigger_level) (struct rtuart *, const int level);
	int (*set_txfifo_trigger_level) (struct rtuart *, const int level);

	int (*get_tx_fifo_size)(struct rtuart *, unsigned int * level); // returns 1 if there is a THR and 0 if there is only the shift register.
	int (*get_rx_fifo_size)(struct rtuart *, unsigned int * level);

	int (*get_tx_fifo_free)(struct rtuart *, unsigned int * level); // how many free space is in the tx fifo.
	int (*get_rx_fifo_level)(struct rtuart *, unsigned int * level); // how many octets are available in the rx fifo.

	int (*write_chars) (struct rtuart *, const u8 * buffer, const size_t count);
	int (*read_chars) (struct rtuart *, u8 * buffer, const size_t count);

	int (*enable_notification)(struct rtuart * uart , const unsigned long notify_mask);
	int (*disable_notification)(struct rtuart * uart , const unsigned long notify_mask);

  	int (*handle_irq)(struct rtuart *);
};

#endif
