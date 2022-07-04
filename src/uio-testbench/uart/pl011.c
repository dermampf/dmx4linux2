// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#include "rtuart.h"
#include "rtuart_client.h"
#include "pl011.h"

#include "kernel.h"

#include <stdio.h> // printf
#include <stdlib.h> // malloc


#include <gpiod.h> // For gpiod_*.

struct rtuart_pl011
{
	struct rtuart uart;
	u8  fcr_shadow;
	u32 base_clock;
	u32 old_input_state; /* datasheet states, that the delta bits are not available. */
	struct {
		u8 fifo[64];
		u8 inptr;
		u8 outptr;
	} rx;


	struct {
		struct gpiod_chip * chip;
		struct gpiod_line * line;
	} gpio_dmxtxen;
};

static inline void pl011_rx_fifo_flush(struct rtuart_pl011 * pl011)
{
	pl011->rx.inptr = pl011->rx.outptr;
}

static inline int pl011_rx_fifo_full(struct rtuart_pl011 * pl011)
{
	return ((pl011->rx.inptr+1)&63) == pl011->rx.outptr;
}

static inline int pl011_rx_fifo_empty(struct rtuart_pl011 * pl011)
{
	return pl011->rx.inptr == pl011->rx.outptr;
}

static inline int pl011_rx_fifo_level(struct rtuart_pl011 * pl011)
{
/*
	if (in == out) size = 0;
	if (in > out)  size = in - out;
	if (in < out)  size = (in+fifosize) - out;
*/
	if (pl011_rx_fifo_empty(pl011))
		return 0;

	if (pl011->rx.inptr > pl011->rx.outptr)
		return pl011->rx.inptr - pl011->rx.outptr;

	return (pl011->rx.inptr + sizeof(pl011->rx.fifo)) - pl011->rx.outptr;
}

static void pl011_rx_fifo_put(struct rtuart_pl011 * pl011, u8 v)
{
	if (!pl011_rx_fifo_full(pl011)) {
		const int p = pl011->rx.inptr;
		pl011->rx.fifo[p] = v;
		pl011->rx.inptr = (p+1) & 63;
	}
}

static int pl011_rx_fifo_get(struct rtuart_pl011 * pl011, u8 * v)
{
	if (!pl011_rx_fifo_empty(pl011)) {
		const int p = pl011->rx.outptr;
		*v = pl011->rx.fifo[p];
		pl011->rx.outptr = (p+1) & 63;
		return 1;
	}
	return 0;
}

enum {
	PL011_REG_DR   = 0/4,
	PL011_REG_RSRECR = 0x4/4,
	PL011_REG_FR   = 0x18/4,
	PL011_REG_IBRD = 0x24/4,
	PL011_REG_FBRD = 0x28/4,
	PL011_REG_LCRH = 0x2C/4,
	PL011_REG_CR   = 0x30/4,
	PL011_REG_IFLS = 0x34/4,
	PL011_REG_IMSC = 0x38/4,
	PL011_REG_RIS  = 0x3C/4,
	PL011_REG_MIS  = 0x40/4,
	PL011_REG_ICR  = 0x44/4,
};


#define PL011_LCRH_SPS    (1<<7) /* stick parity */
#define	PL011_LCRH_WLEN_SHIFT 5 /* n=5..8 */
#define	PL011_LCRH_WLEN_MASK (3<<PL011_LCRH_WLEN_SHIFT)
#define	PL011_LCRH_FEN    (1<<4) /* fifo enable */
#define	PL011_LCRH_STP2   (1<<3) /* 1 = two stop bits, 0 = one stop bit */
#define	PL011_LCRH_EPS    (1<<2) /* 1 = even parity, 0 = odd partiy */
#define	PL011_LCRH_PEN    (1<<1) /* 1 = parity enabled, 0 = parity disabled */
#define	PL011_LCRH_BRK    (1<<0) /* 1 = break on, 0 = break off */


enum {
	PL011_CR_CTSEN = (1<<15),
	PL011_CR_RTSEN = (1<<14),
	PL011_CR_OUT2  = (1<<13),
	PL011_CR_OUT1  = (1<<12),
	PL011_CR_RTS   = (1<<11),
	PL011_CR_DTR   = (1<<10),
	PL011_CR_RXE   = (1<<9), /* Receiver enable */
	PL011_CR_TXE   = (1<<8), /* Transmitter enable */
	PL011_CR_LBE   = (1<<7), /* Loopback enable */
	PL011_CR_SIRLP = (1<<2),
	PL011_CR_SIREN = (1<<1),
	PL011_CR_UARTEN = (1<<0), /* Uart enable */
};

enum {
	PL011_MIS_OE = (1<<10),
	PL011_MIS_BE = (1<<9),
	PL011_MIS_PE = (1<<8),
	PL011_MIS_FE = (1<<7),
	PL011_MIS_RT = (1<<6),
	PL011_MIS_TX = (1<<5),
	PL011_MIS_RX = (1<<4),
	PL011_MIS_CTS = (1<<1)
};

enum {
	PL011_IS_OE  = PL011_MIS_OE,
	PL011_IS_BE  = PL011_MIS_BE,
	PL011_IS_PE  = PL011_MIS_PE,
	PL011_IS_FE  = PL011_MIS_FE,
	PL011_IS_RT  = PL011_MIS_RT,
	PL011_IS_TX  = PL011_MIS_TX,
	PL011_IS_RX  = PL011_MIS_RX,
	PL011_IS_CTS = PL011_MIS_CTS
};

enum { // PL011_REG_FR
	PL011_FR_TXFE = (1<<7),
	PL011_FR_RXFF = (1<<6),
	PL011_FR_TXFF = (1<<5),
	PL011_FR_RXFE = (1<<4),
	PL011_FR_BUSY = (1<<3),
	PL011_FR_CTS  = (1<<0),
};

static inline int PL011_FR_ISTXFIFOEMPTY(const int fr)
    { return fr & PL011_FR_TXFE; }
static inline int PL011_FR_ISRXFIFOFULL(const int fr)
    { return fr & PL011_FR_RXFF; }
static inline int PL011_FR_ISTXFIFOFULL(const int fr)
    { return fr & PL011_FR_TXFF; }
static inline int PL011_FR_ISRXFIFOEMPTY(const int fr)
    { return fr & PL011_FR_RXFE; }
static inline int PL011_FR_ISTXBUSY(const int fr)
    { return fr & PL011_FR_BUSY; }


static unsigned int pl011_irqstatus_to_event(const u32 is)
{
	// later use uint64_t and use   return (LsrToEvent >> ((lsr>>1)&15)) & 15;
	// or use array of 16 uint8_t and do return LsrToEvent[(lsr>>1)&15];
	return  ((is & PL011_IS_OE) ? RTUART_EVENT_OVERRUN_ERROR : 0) |
		((is & PL011_IS_PE) ? RTUART_EVENT_PARITY_ERROR : 0) |
		((is & PL011_IS_FE) ? RTUART_EVENT_FRAMING_ERROR : 0) |
		((is & PL011_IS_BE) ? RTUART_EVENT_BREAK : 0)
		;
}

/* parity: 'n','e','o', '1', '0'. stopbits: 1=10, 1,5=15, 2=20 */
static int pl011_set_format(struct rtuart *u, const int databits, const char parity, const int stopbits)
{
// DONE
	u32 lcr;
	rtuart_read_u32(u, PL011_REG_LCRH, &lcr);

	lcr &= ~PL011_LCRH_WLEN_MASK;
	lcr |= ((databits-5)<<PL011_LCRH_WLEN_SHIFT) & PL011_LCRH_WLEN_MASK;

	lcr &= ~(PL011_LCRH_PEN|PL011_LCRH_EPS);
	lcr |= (parity=='e')
		? (PL011_LCRH_PEN|PL011_LCRH_EPS)
		: (parity=='o') ? PL011_LCRH_PEN
		: 0 ;

	lcr &= ~PL011_LCRH_STP2;
	lcr |= (stopbits > 10) ? PL011_LCRH_STP2 : 0;
	rtuart_write_u32(u, PL011_REG_LCRH, lcr);
	return 0;
}

static int pl011_get_format(struct rtuart *u, int * databits, char * parity, int * stopbits)
{
//DONE
	u32 lcr;
	rtuart_read_u32(u, PL011_REG_LCRH, &lcr);
	*databits = ((lcr>>5)&3) + 5;
	*parity = (lcr & PL011_LCRH_PEN) ? ((lcr & PL011_LCRH_EPS) ? 'e' : 'o' ) : 'n';
	*stopbits = (lcr & PL011_LCRH_STP2) ? 20 : 10;
	return 0;
}

static int pl011_set_baudrate(struct rtuart *u, const unsigned int baudrate)
{
//DONE
	struct rtuart_pl011 * uart = container_of(u, struct rtuart_pl011, uart);
	const u32 divisor = (uart->base_clock * 64) / (16 * baudrate);
	rtuart_write_u32(u, PL011_REG_IBRD, (u16)(divisor >> 6));
	rtuart_write_u32(u, PL011_REG_FBRD, divisor & 0x63);
	u32 lcrh;
	if (rtuart_read_u32(u, PL011_REG_LCRH, &lcrh) == 0)
		rtuart_write_u32(u, PL011_REG_LCRH, lcrh);
	return 0;
}

static int pl011_get_baudrate(struct rtuart *u, unsigned int * baudrate)
{
//DONE
	struct rtuart_pl011 * uart = container_of(u, struct rtuart_pl011, uart);
	u32 ibrd, fbrd;
	rtuart_read_u32(u, PL011_REG_IBRD, &ibrd);
	rtuart_read_u32(u, PL011_REG_FBRD, &fbrd);
	const u32 divisor = ((ibrd&0xffff)<<6) | (fbrd & 0x3f);
	*baudrate  = (uart->base_clock * 64) / (16 * divisor);
	return 0;
}

static int pl011_set_break(struct rtuart *u, const int on)
{
	printf ("#### pl011_set_break %d\n", on);
//DONE
	u32 lcrh;
	rtuart_read_u32(u, PL011_REG_LCRH, &lcrh);
	if (on)
		lcrh |= PL011_LCRH_BRK;
	else
		lcrh &= ~PL011_LCRH_BRK;
	rtuart_write_u32(u, PL011_REG_LCRH, lcrh);
	return 0;
}

static int pl011_get_break(struct rtuart *u, int *on)
{
//DONE
	u32 lcrh;
	rtuart_read_u32(u, PL011_REG_LCRH, &lcrh);
	*on = (lcrh & PL011_LCRH_BRK) ? 1 : 0;
	return 0;
}

static int pl011_get_line_status(struct rtuart *u, unsigned int * line)
{
	return -1; // not implemented yet.
	//u8 l;
	//const int ret = rtuart_read_u8(u, PC16550_REG_LSR, &l);
	//*line = l;
	//return ret;
}

static int pl011_set_modem_lines(struct rtuart *u,
				    unsigned int _mask,
				    unsigned int _line)
{
// DONE
	struct rtuart_pl011 * uart = container_of(u, struct rtuart_pl011, uart);
	u32 mask = 0;
	u32 line = 0;
	u32 cr;
	if (_mask & RTUART_OUTPUT_RTS) mask |= PL011_CR_RTS;
	if (_line & RTUART_OUTPUT_RTS) line |= PL011_CR_RTS;
	if (_mask & RTUART_OUTPUT_DTR) mask |= PL011_CR_DTR;
	if (_line & RTUART_OUTPUT_DTR) line |= PL011_CR_DTR;

	if (_mask & RTUART_OUTPUT_GENERIC0) mask |= PL011_CR_OUT1;
	if (_line & RTUART_OUTPUT_GENERIC0) line |= PL011_CR_OUT1;

	if (_mask & RTUART_OUTPUT_GENERIC1) mask |= PL011_CR_OUT2;
	if (_line & RTUART_OUTPUT_GENERIC1) line |= PL011_CR_OUT2;

	if (_mask & RTUART_OUTPUT_RTS) {
		if (gpiod_line_set_value(uart->gpio_dmxtxen.line,
					 (_line & RTUART_OUTPUT_RTS) ? 0 : 1))
			printf ("failed to set RTS gpio\n");
	}

	if (rtuart_read_u32(u, PL011_REG_CR, &cr))
		return -1;
	cr &= ~mask;
	cr |= mask & line;
	return rtuart_write_u32(u, PL011_REG_CR, cr);
}

static int pl011_set_fifo_enable(struct rtuart *u, const int on)
{
	struct rtuart_pl011 * uart = container_of(u, struct rtuart_pl011, uart);
	u32 cr;
	if (rtuart_read_u32(u, PL011_REG_LCRH, &cr))
		return -1;
	if (on)
		cr |= PL011_LCRH_FEN;
	else
		cr &= ~PL011_LCRH_FEN;
	return rtuart_write_u32(u, PL011_REG_LCRH, cr);
	// we need to flush the fifos.
}

static int pl011_flush_fifo (struct rtuart *u, const int flush_tx, const int flush_rx)
{
// DONE
	struct rtuart_pl011 * uart = container_of(u, struct rtuart_pl011, uart);

	if (flush_tx) {
		u32 cr;
		if (rtuart_read_u32(u, PL011_REG_CR, &cr))
			return -1;
		if (cr & (1<<4)) {
			/*
			 * The datasheet states that disabling the fifo flushes the txfifo
			 */
			rtuart_write_u32(u, PL011_REG_CR, cr & ~(1<<4));
			rtuart_write_u32(u, PL011_REG_CR, cr);
		}
	}
	if (flush_rx)
		pl011_rx_fifo_flush(uart);

	return 0;
}

static int pl011_fifo_level_to_code(const int level)
{
	int v = 0;
	switch (level)
	{
	case 16*7/8:
		v++;
	case 16*3/4:
		v++;
	case 16/2:
		v++;
	case 16/4:
		v++;
	case 16/8:
		break;
	default:
		v = -1;
		break;
	}
	return v;
}

static int pl011_set_fifo_trigger_level (struct rtuart *u, const int level, const int bitOffset)
{
	struct rtuart_pl011 * uart = container_of(u, struct rtuart_pl011, uart);
	u32 ifls;
	const int rxcode = pl011_fifo_level_to_code(level);
	if ((rxcode >= 0) && (0==rtuart_read_u32(u, PL011_REG_IFLS, &ifls))) {
		ifls &= ~ (7<<bitOffset);
		ifls |= rxcode << bitOffset;
		rtuart_write_u32(u, PL011_REG_IFLS, ifls);
		printf ("set fifo level: ifls=%08lX\n", ifls);
		return 0;
	}
	return -1;
}

static int pl011_set_rxfifo_trigger_level (struct rtuart *u, const int level)
{
	return pl011_set_fifo_trigger_level (u, level, 3);
}

static int pl011_set_txfifo_trigger_level (struct rtuart *u, const int level)
{
	return pl011_set_fifo_trigger_level (u, level, 0);
}


// returns 1 if there is a THR and 0 if there is only the shift register.
static int pl011_get_tx_fifo_size(struct rtuart *u, unsigned int * level)
{
	u32 lcrh;
	if (rtuart_read_u32(u, PL011_REG_LCRH, &lcrh))
		return -1;
	*level = (lcrh & PL011_LCRH_FEN) ? 16 : 1;
	return 0;
}

static int pl011_get_rx_fifo_size(struct rtuart *u, unsigned int * level)
{
	return pl011_get_tx_fifo_size(u, level);
}

// how many free space is in the tx fifo.
static int pl011_get_tx_fifo_free(struct rtuart *u, unsigned int * level)
{
	u32 fr;
	if (rtuart_read_u32(u, PL011_REG_FR, &fr))
		return -1;
	*level = (fr&(1<<7)) ? 16 : (fr&(1<<5)) ? 0 : 1;
	return 0;
}

// how many octets are available in the rx fifo.
static int pl011_get_rx_fifo_level(struct rtuart *u, unsigned int * level)
{
	return -1;
}

/*
 * Writes the number of octetes to the tx-fifo regardless if space is available or not.
 */
static int pl011_write_chars (struct rtuart *u, const u8 * buffer, const size_t count)
{
	size_t c = 0;
	while (c < count) {
		u32 fr;
		if (rtuart_read_u32(u, PL011_REG_FR, &fr))
			return -1;
		if (PL011_FR_ISTXFIFOFULL(fr))
			break;
		rtuart_write_u32(u, 0, *(buffer++));
		++c;
	}
	return 0;
}

static int pl011_read_chars (struct rtuart *u, u8 * buffer, const size_t count)
{
// DONE
	printf ("pl011_read_chars (count:%lu)\n", count);
	struct rtuart_pl011 * uart = container_of(u, struct rtuart_pl011, uart);
	size_t c = 0;
	while (c < count)
	{
		if (!pl011_rx_fifo_get(uart, buffer))
			return c;
		++buffer;
		++c;
	}
	return c;
}


//============ IRQ Handler ================
static void pl011_uart_update_notification(struct rtuart * u);

static void pl011_handle_receiver_empty(struct rtuart_pl011 * uart)
{
// DONE
	struct rtuart * u = &uart->uart;
	u32 fr;
	if (rtuart_read_u32(u, PL011_REG_FR, &fr))
		return;

	printf ("pl011_handle_receiver_empty fr:%08lX txempty:%d txbusy:%d n:%02X\n",
		fr,
		PL011_FR_ISRXFIFOEMPTY(fr),
		PL011_FR_ISTXBUSY(fr),
		u->notify_mask
		);

	if (PL011_FR_ISTXFIFOEMPTY(fr) && (u->notify_mask & UART_NOTIFY_TXEMPTY))
	{
		u->notify_mask &= ~UART_NOTIFY_TXEMPTY;
		if (PL011_FR_ISTXBUSY(fr))
		{
			if (u->client && u->client->callbacks && u->client->callbacks->transmitter_empty)
				u->client->callbacks->transmitter_empty(u);
		}
		else
		{
			// TODO: cheating implementation that sleep. Works for as long as we are in userspace.
			if (u->client && u->client->callbacks && u->client->callbacks->transmitter_empty)
			{
				// Linux: schedule_delayed_task(uart->rxempty_task);


				while ((0==rtuart_read_u32(u, PL011_REG_FR, &fr)) && PL011_FR_ISTXBUSY(fr)) {
					usleep(88);
				}
				u->client->callbacks->transmitter_empty(u);
			}
		}
	}
}


static void pl011_handle_lsr(struct rtuart_pl011 * uart, const u32 is)
{
	struct rtuart * u = &uart->uart;
	/* This is completely different to the 8250 uart */
	const int ev = pl011_irqstatus_to_event(is);
	if (ev && u->client && u->client->callbacks && u->client->callbacks->line_status_event)
	{
		u->client->callbacks->line_status_event(u, ev);
	}
}



static void pl011_rxempty_task_handler(struct rtuart * u)
{
// DONE
	if (u->notify_mask & UART_NOTIFY_TXEMPTY)
	{
		struct rtuart_pl011 * uart = container_of(u, struct rtuart_pl011, uart);
		u32 mis;
		if (!rtuart_read_u32(u, PL011_REG_MIS, &mis))
			pl011_handle_lsr(uart, mis);
		pl011_handle_receiver_empty(uart);
	}
}

// may have interrup handler in a seperate file
static int pl011_handle_irq(struct rtuart *u)
{
	struct rtuart_pl011 * uart = container_of(u, struct rtuart_pl011, uart);
	while (1)
	{
		u32 mis;
		rtuart_read_u32(u, PL011_REG_MIS, &mis);

		if (0==(mis & (PL011_MIS_OE|PL011_MIS_BE|PL011_MIS_PE|PL011_MIS_FE|PL011_MIS_RT|PL011_MIS_TX|PL011_MIS_RX|PL011_MIS_CTS))) {
			pl011_handle_receiver_empty(uart);
		        pl011_uart_update_notification(u);
			printf ("pl011_handle_irq:exit\n");
			fflush(stdout);
			return 0;
		}

		if (mis & (PL011_MIS_OE | PL011_MIS_BE | PL011_MIS_PE | PL011_MIS_FE) ) {
			rtuart_write_u32(u, PL011_REG_ICR, PL011_MIS_OE | PL011_MIS_BE | PL011_MIS_PE | PL011_MIS_FE);
			printf ("pl011_handle_irq:lsr\n");
			pl011_handle_lsr(uart, mis);
		}

		if (mis & (PL011_MIS_RX|PL011_MIS_RT)) {
			u32 error = 0;
			// Receiver available: read as much as is available and call recive function.
			u32 fr;
			rtuart_write_u32(u, PL011_REG_ICR, PL011_MIS_RX|PL011_MIS_RT);
			printf ("pl011_handle_irq:rx\n");
			while (!rtuart_read_u32(u, PL011_REG_FR, &fr) &&
			       !PL011_FR_ISRXFIFOEMPTY(fr)
				) {
				// RX-Fifo not empty
				u32 dr;
				if (!rtuart_read_u32(u, PL011_REG_DR, &dr)) {
					/* Bail out and handle as much 
					 * data as was available until the error.
					 * Handle the error later. It is important
					 */
					error = dr & 0xF00;
					if (error)
						break;
					pl011_rx_fifo_put(uart, (u8)dr);
				}
			}
			if (u->notify_mask & UART_NOTIFY_RXAVAILABLE)
			{
				const unsigned int rxfifo_level = pl011_rx_fifo_level(uart);
				if (rxfifo_level) {
					u->notify_mask &= ~ UART_NOTIFY_RXAVAILABLE;

					if (u->client && u->client->callbacks && u->client->callbacks->receiver_data_available)
						u->client->callbacks->receiver_data_available(u, rxfifo_level);
					else
						pl011_rx_fifo_flush(uart);
				}
			}
			else
				pl011_rx_fifo_flush(uart);

			/* Now it is time to handle the error */
			if (error) {
				int ev = 0;
				if (error & (1<<11))
					ev |= RTUART_EVENT_OVERRUN_ERROR;
				if (error & (1<<9))
					ev |= RTUART_EVENT_PARITY_ERROR;
				if (error & (1<<8))
					ev |= RTUART_EVENT_FRAMING_ERROR;
				if (error & (1<<10))
					ev |= RTUART_EVENT_BREAK;

				if (ev && u->client && u->client->callbacks && u->client->callbacks->line_status_event)
				{
					u->client->callbacks->line_status_event(u, ev);
				}
			}
			break;

		}

		if (mis & PL011_MIS_TX) {
			rtuart_write_u32(u, PL011_REG_ICR, PL011_MIS_TX);
			printf ("pl011_handle_irq:tx\n");
		  	if (u->notify_mask & UART_NOTIFY_TXREADY)
			{
				u->notify_mask &= ~UART_NOTIFY_TXREADY;
				if (u->client && u->client->callbacks && u->client->callbacks->transmitter_has_space)
				{
					unsigned int space = 0;
					if (0==rtuart_get_tx_fifo_size(u, &space))
						u->client->callbacks->transmitter_has_space(u, space);
				}
			}
		}

		if (mis & PL011_MIS_CTS) {
			rtuart_write_u32(u, PL011_REG_ICR, PL011_MIS_CTS);
			printf ("pl011_handle_irq:cts\n");
			// TODO: we need to fix handling here, the values given the callback are nonsense. It is just a pseudo implementation.
			int cts_value = 1; // read value here.
			if ((uart->old_input_state != cts_value) &&
			    u->client &&
			    u->client->callbacks &&
			    u->client->callbacks->modem_input_changed)
				u->client->callbacks->modem_input_changed(
					u,
					1,
					cts_value);
			uart->old_input_state = cts_value;
		}
	}
	return 0;
}

static void pl011_uart_update_notification(struct rtuart * u)
{
	int start_tx = 0;
	u32 cr;
	u32 imsc = 0;
	if (u->notify_mask & UART_NOTIFY_RXAVAILABLE)
		imsc |= (1<<4) | (1<<6);
	// if (u->notify_mask & UART_NOTIFY_TXEMPTY)

	if (u->notify_mask & UART_NOTIFY_TXREADY) {
		start_tx = ((imsc & (1<<5)) == 0);
		imsc |= (1<<5);
	}

	if (u->notify_mask & UART_NOTIFY_LINEEVENT)
		imsc |= (0xF<<7); // Overrun, Break, Parity, Frame

	if (u->notify_mask & UART_NOTIFY_INPUTCHANGED)
		imsc |= (1<<1);

	rtuart_write_u32(u, PL011_REG_IMSC, imsc);

	/* Enable or disable Transmitter and Receiver
	 * as they are needed.
	 */
	if (0==rtuart_read_u32(u, PL011_REG_CR, &cr)) {
#if 0
		cr |= PL011_CR_RXE | PL011_CR_TXE;
#else
		if (u->notify_mask & (UART_NOTIFY_RXAVAILABLE|UART_NOTIFY_LINEEVENT))
			cr |= PL011_CR_RXE;
		else
			cr &= ~PL011_CR_RXE;

		/* we need to have the transmitter enabled always, as we need it to change break */
		cr |= PL011_CR_TXE;
#endif
		rtuart_write_u32(u, PL011_REG_CR, cr);
	}
	printf ("update_notification(0x%02lX) -> 0x%08lX\n", u->notify_mask, imsc);
#if 1
	if (start_tx) {
		/* The transmision has just been switched on.
		 * On the PC16C550 it initiates a TX-Read interrupt.
		 * Lets just simulate it here.
		 */
		if (u->client && u->client->callbacks && u->client->callbacks->transmitter_has_space)
		{
			unsigned int space = 0;
			if (0==rtuart_get_tx_fifo_size(u, &space)) {
				u->client->callbacks->transmitter_has_space(u, space);
			}
		}
	}
#endif
}

/* Notification will be automatically disabled once the notifier function is called. */
static int pl011_uart_enable_notification(struct rtuart * u, const unsigned long notify_mask)
{
// DONE
	// may make this atomic.
	if ((u->notify_mask & notify_mask) != notify_mask)
	{
		u->notify_mask |= notify_mask;
		pl011_uart_update_notification(u);
	}
	// else printf ("pl011_uart_enable_notification nochange\n");
	return 0;
}

static int pl011_uart_disable_notification(struct rtuart * u, const unsigned long notify_mask)
{
// DONE
	// may make this atomic.
	if ((u->notify_mask & notify_mask) != notify_mask)
	{
		u->notify_mask &= ~notify_mask;
		pl011_uart_update_notification(u);
	}
	// else printf ("pl011_uart_disable_notification nochange\n");
	return 0;
}

//===========================================================



struct rtuart_ops pl011_ops =
{
	.set_format = pl011_set_format,
	.get_format = pl011_get_format,
	.set_baudrate = pl011_set_baudrate,
	.get_baudrate = pl011_get_baudrate,
	.set_break = pl011_set_break,
	.get_break = pl011_get_break,
	.get_line_status = pl011_get_line_status,
	.set_modem_lines = pl011_set_modem_lines,
	.set_fifo_enable = pl011_set_fifo_enable,
	.flush_fifo = pl011_flush_fifo,
	.set_rxfifo_trigger_level = pl011_set_rxfifo_trigger_level,
	.set_txfifo_trigger_level = pl011_set_txfifo_trigger_level,
	.get_tx_fifo_size = pl011_get_tx_fifo_size,
	.get_rx_fifo_size = pl011_get_rx_fifo_size,
	.get_tx_fifo_free = pl011_get_tx_fifo_free,
	.get_rx_fifo_level = pl011_get_rx_fifo_level,
	.write_chars = pl011_write_chars,
	.read_chars = pl011_read_chars,
	.enable_notification = pl011_uart_enable_notification,
	.disable_notification = pl011_uart_disable_notification,
	.handle_irq = pl011_handle_irq
};

struct rtuart * pl011_create(const unsigned long base_clock)
{
	struct rtuart_pl011 * u = (struct rtuart_pl011 *)malloc(sizeof(struct rtuart_pl011));
	if (u)
	{
		u->uart.ops = &pl011_ops;
		u->fcr_shadow = 0;
		u->base_clock = base_clock;

		u->gpio_dmxtxen.chip = gpiod_chip_open_by_name("gpiochip0");
		if (u->gpio_dmxtxen.chip) {
			u->gpio_dmxtxen.line = gpiod_chip_get_line(u->gpio_dmxtxen.chip, 18);
			if (0 == u->gpio_dmxtxen.line) {
				gpiod_chip_close(u->gpio_dmxtxen.chip);
				u->gpio_dmxtxen.chip = 0;
				printf ("!!!!!!! failed to get gpio0.18\n");
			}
			else if (gpiod_line_request_output(u->gpio_dmxtxen.line, "dmxtxen", 0)) {
				printf ("!!!!!!! failed to request gpio0.18 as output\n");
				gpiod_line_release(u->gpio_dmxtxen.line);
				gpiod_chip_close(u->gpio_dmxtxen.chip);
			}
		}
		else
			printf ("!!!!!!! failed to create gpio0\n");

	}
	return &u->uart;
}

/* pl011_cleanup()
{
gpiod_line_release(u->gpio_dmxtxen.line);
gpiod_chip_close(u->gpio_dmxtxen.chip);
}
*/

//TODO: remove this once moved to kernel -- in the kernel use a bottomhalf that triggers itself or a timer.
void pl011_periodic_check(struct rtuart * u)
{
	pl011_rxempty_task_handler(u);
}
