// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#include "rtuart.h"
#include "rtuart_client.h"
#include "pc16c550.h"

#include "kernel.h"

#include <stdio.h> // printf
#include <stdlib.h> // malloc

struct rtuart_pc16c550
{
	struct rtuart uart;
	u8  fcr_shadow;
	u32 base_clock;
};

enum {
	PC16550_REG_RBR = 0,
	PC16550_REG_THR = 0,
	PC16550_REG_IER = 1,
	PC16550_REG_IIR = 2,
	PC16550_REG_FCR = 2,
	PC16550_REG_LCR = 3, // DLAB = LCR.7
	PC16550_REG_MCR = 4,
	PC16550_REG_LSR = 5,
	PC16550_REG_MSR = 6,
	PC16550_REG_SCR = 7,
	PC16550_REG_DLL = 0,
	PC16550_REG_DLM = 1
};

enum
{
	PC16550_LSR_DATA_READY    = (1<<0),
	PC16550_LSR_OVERRUN_ERROR = (1<<1),
	PC16550_LSR_PARITY_ERROR  = (1<<2),
	PC16550_LSR_FRAMING_ERROR = (1<<3),
	PC16550_LSR_BREAK_INTERRUPT = (1<<4),
	PC16550_LSR_THRE            = (1<<5), // Transmitter Holding Register Empty
	PC16550_LSR_TRANSMITTER_EMPTY = (1<<6),
	PC16550_LSR_ERROR_IN_RXFIFO   = (1<<7),
};

static unsigned int uart16550_lsr_to_event(const u8 lsr)
{
	// later use uint64_t and use   return (LsrToEvent >> ((lsr>>1)&15)) & 15;
	// or use array of 16 uint8_t and do return LsrToEvent[(lsr>>1)&15];
	return  ((lsr & PC16550_LSR_OVERRUN_ERROR) ? RTUART_EVENT_OVERRUN_ERROR : 0) |
		((lsr & PC16550_LSR_PARITY_ERROR) ? RTUART_EVENT_PARITY_ERROR : 0) |
		((lsr & PC16550_LSR_FRAMING_ERROR) ? RTUART_EVENT_FRAMING_ERROR : 0) |
		((lsr & PC16550_LSR_BREAK_INTERRUPT) ? RTUART_EVENT_BREAK : 0)
		;
}



/* parity: 'n','e','o', '1', '0'. stopbits: 1=10, 1,5=15, 2=20 */
static int pc16c550_set_format(struct rtuart *u, const int databits, const char parity, const int stopbits)
{
	u8 lcr;
	rtuart_read_u8(u, PC16550_REG_LCR, &lcr);
	lcr &= ~0x3f;
	lcr |= databits-5;
	lcr |= (stopbits > 10) ? 4 : 0;
	lcr |= ( (parity=='e') ? 3 : (parity=='o') ? 1 : 0 ) << 3;
	rtuart_write_u8(u, PC16550_REG_LCR, lcr);
	return 0;
}

static int pc16c550_get_format(struct rtuart *u, int * databits, char * parity, int * stopbits)
{
	u8 lcr;
	rtuart_read_u8(u, PC16550_REG_LCR, &lcr);
	*databits = (lcr&3) + 5;
	*parity = (lcr & 8) ? ((lcr & 0x10) ? 'e' : 'o' ) : 'n';
	*stopbits = (lcr & 4) ? 20 : 10;
	return 0;
}

static int pc16c550_set_baudrate(struct rtuart *u, const unsigned int baudrate)
{
	struct rtuart_pc16c550 * uart = container_of(u, struct rtuart_pc16c550, uart);
	// lock uart
	const int oversampling = 16;
        const unsigned int divisor = (uart->base_clock / oversampling) / baudrate;
	u8 lcr;
	rtuart_read_u8(u, PC16550_REG_LCR, &lcr);
	rtuart_write_u8(u, PC16550_REG_LCR, lcr | (1<<7));
	rtuart_write_u8(u, PC16550_REG_DLL, (u8)divisor);
	rtuart_write_u8(u, PC16550_REG_DLM, (u8)(divisor>>8));
	rtuart_write_u8(u, PC16550_REG_LCR, lcr);
	// unlock uart
	return 0;
}

static int pc16c550_get_baudrate(struct rtuart *u, unsigned int * baudrate)
{
	struct rtuart_pc16c550 * uart = container_of(u, struct rtuart_pc16c550, uart);
	// lock uart
	u8 divisor_msb, divisor_lsb;
	u8 lcr;
	const int oversampling = 16;
	rtuart_read_u8(u, PC16550_REG_LCR, &lcr);
	rtuart_write_u8(u, PC16550_REG_LCR, lcr | (1<<7));
	rtuart_read_u8(u, PC16550_REG_DLL, &divisor_lsb);
	rtuart_read_u8(u, PC16550_REG_DLM, &divisor_msb);
	rtuart_write_u8(u, PC16550_REG_LCR, lcr);
	*baudrate = (uart->base_clock / oversampling) / (divisor_msb<<8 + divisor_lsb);
	// unlock uart
	return 0;
}

static int pc16c550_set_break(struct rtuart *u, const int on)
{
	u8 lcr;
	rtuart_read_u8(u, PC16550_REG_LCR, &lcr);
	if (on)
		lcr |= 0x40;
	else
		lcr &= ~0x40;
	rtuart_write_u8(u, PC16550_REG_LCR, lcr);
	return 0;
}

static int pc16c550_get_break(struct rtuart *u, int *on)
{
	u8 lcr;
	rtuart_read_u8(u, PC16550_REG_LCR, &lcr);
	*on = (lcr & 0x40) ? 1 : 0;
	return 0;
}

static int pc16c550_get_line_status(struct rtuart *u, unsigned int * line)
{
	u8 l;
	const int ret = rtuart_read_u8(u, PC16550_REG_LSR, &l);
	*line = l;
	return ret;
}

static int pc16c550_set_modem_lines(struct rtuart *u,
				    unsigned int mask,
				    unsigned int line)
{
	u8 mcr;
	if (rtuart_read_u8(u, PC16550_REG_MCR, &mcr))
		return -1;
	mcr &= ~mask;
	mcr |= mask & line;
	// mcr = (mcr & ~mask) | (line & mask);
	// RTUART_OUTPUT_DTR, RTUART_OUTPUT_RTS, RTUART_OUTPUT_GENERIC0, RTUART_OUTPUT_GENERIC1
	return rtuart_write_u8(u, PC16550_REG_MCR, mcr);
}

static int pc16c550_set_fifo_enable(struct rtuart *u, const int on)
{
	struct rtuart_pc16c550 * uart = container_of(u, struct rtuart_pc16c550, uart);
	if (on)
		uart->fcr_shadow |= 1;
	else
		uart->fcr_shadow &= ~1;
	// flush tx and rx fifo on fifo enable.
	rtuart_write_u32(u, PC16550_REG_FCR, uart->fcr_shadow | (on ? 6 : 0));
	return 0;
}

static int pc16c550_flush_fifo (struct rtuart *u, const int flush_tx, const int flush_rx)
{
	struct rtuart_pc16c550 * uart = container_of(u, struct rtuart_pc16c550, uart);
	rtuart_write_u32(u, PC16550_REG_FCR, uart->fcr_shadow | (flush_tx?4:0) | (flush_rx ? 2 : 0));
	return 0;
}

static int pc16c550_set_rxfifo_trigger_level (struct rtuart *u, const int level)
{
	struct rtuart_pc16c550 * uart = container_of(u, struct rtuart_pc16c550, uart);
	int v = 0;
	switch (level)
	{
	case 14:
		v++;
	case 8:
		v++;
	case 4:
		v++;
	case 1:
		uart->fcr_shadow = (uart->fcr_shadow & ~0xC0) | (v<<6);
		printf ("set rxfifo level: fcr=%02X\n", uart->fcr_shadow);
		rtuart_write_u32(u, PC16550_REG_FCR, uart->fcr_shadow);
		break;
	default:
		return -1;
	}
	return 0;
}

static int pc16c550_set_txfifo_trigger_level (struct rtuart *u, const int level)
{
	return -1;
}

// returns 1 if there is a THR and 0 if there is only the shift register.
static int pc16c550_get_tx_fifo_size(struct rtuart *u, unsigned int * level)
{
	(void)u;
	// *level = (!rtuart_write_u8(u, 1, &fifo) && (fifo0xC0 == 0xC0)) ? 16 : 1;

	*level = 16;
	return 0;
}

static int pc16c550_get_rx_fifo_size(struct rtuart *u, unsigned int * level)
{
	return pc16c550_get_tx_fifo_size(u, level);
}

// how many free space is in the tx fifo.
static int pc16c550_get_tx_fifo_free(struct rtuart *u, unsigned int * level)
{
	return -1;
}

// how many octets are available in the rx fifo.
static int pc16c550_get_rx_fifo_level(struct rtuart *u, unsigned int * level)
{
	return -1;
}

/*
 * Writes the number of octetes to the tx-fifo regardless if space is available or not.
 */
static int pc16c550_write_chars (struct rtuart *u, const u8 * buffer, const size_t count)
{
	size_t c = count;
	while (c-- > 0)
		rtuart_write_u8(u, 0, *(buffer++));
	return 0;
}

/*
 * Reads the number of octetes from the rx-fifo regardless if they are available or not.
 */
static int pc16c550_read_chars (struct rtuart *u, u8 * buffer, const size_t count)
{
	size_t c = count;
	while (c-- > 0)
	{
		rtuart_read_u8(u, 0, buffer);
		++buffer;
	}
	return count;
}


//============ IRQ Handler ================
static void pc16c550_uart_update_notification(struct rtuart * u);

static void uart16550_handle_lsr(struct rtuart_pc16c550 * uart)
{
	struct rtuart * u = &uart->uart;
	u8 lsr, rhr;
	rtuart_read_u8(u, PC16550_REG_LSR, &lsr);
	if (lsr & PC16550_LSR_BREAK_INTERRUPT) // break interrupt - we also receive a 0-char -> dummy read.
		rtuart_read_u8(u, PC16550_REG_RBR, &rhr);

	if ((lsr&(15<<1)) && u->client && u->client->callbacks && u->client->callbacks->line_status_event)
	{
		u->client->callbacks->line_status_event(u, uart16550_lsr_to_event(lsr));
	}

	if ((lsr & PC16550_LSR_THRE) && (u->notify_mask & UART_NOTIFY_TXEMPTY))
	{
		u->notify_mask &= ~UART_NOTIFY_TXEMPTY;
		if (lsr & PC16550_LSR_TRANSMITTER_EMPTY)
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
				while (!rtuart_read_u8(u, PC16550_REG_LSR, &lsr) && !(lsr & PC16550_LSR_TRANSMITTER_EMPTY))
					usleep(88);
				u->client->callbacks->transmitter_empty(u);
			}
		}
	}
}

static void uart16550_rxempty_task_handler(struct rtuart * u)
{
	if (u->notify_mask & UART_NOTIFY_TXEMPTY)
	{
		struct rtuart_pc16c550 * uart = container_of(u, struct rtuart_pc16c550, uart);
		uart16550_handle_lsr(uart);
	}
}

// may have interrup handler in a seperate file
static int pc16c550_handle_irq(struct rtuart *u)
{
	struct rtuart_pc16c550 * uart = container_of(u, struct rtuart_pc16c550, uart);
	while (1)
	{
		u8 iir, rhr, msr, lsr;
		rtuart_read_u8(u, PC16550_REG_IIR, &iir);
		//printf ("iir:%02X\n", iir);
		switch (iir & 0xf)
		{
		case 1:
			//printf ("IDLE\n");
			uart16550_handle_lsr(uart);
		        pc16c550_uart_update_notification(u);
			return 0;
			break;

		case 0: // modem status
			rtuart_read_u8(u, PC16550_REG_MSR, &msr);
			if (u->client && u->client->callbacks && u->client->callbacks->modem_input_changed)
				u->client->callbacks->modem_input_changed(u, (msr>>4)&15, msr&15);
			break;

		case 2: // THR empty
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
			//uart16550_handle_lsr(uart);
			break;

		case 4: // RX Data Available
			if (u->notify_mask & UART_NOTIFY_RXAVAILABLE)
			{
				u->notify_mask &= ~ UART_NOTIFY_RXAVAILABLE;
				const unsigned int rxfifo_trigger_level = 8;
				if (u->client && u->client->callbacks && u->client->callbacks->receiver_data_available)
					u->client->callbacks->receiver_data_available(u, rxfifo_trigger_level);
				else
					rtuart_flush_fifo (u, 0, 1); // flush rx
			}
			else
				rtuart_flush_fifo (u, 0, 1); // flush rx
			break;

		case 0xC: // RX character timeout
			if (u->notify_mask & UART_NOTIFY_RXAVAILABLE)
			{
				u->notify_mask &= ~ UART_NOTIFY_RXAVAILABLE;
				if (u->client && u->client->callbacks && u->client->callbacks->receiver_data_available)
					u->client->callbacks->receiver_data_available(u, -1);
				else
					rtuart_flush_fifo (u, 0, 1); // flush rx
			}
			else
				rtuart_flush_fifo (u, 0, 1); // flush rx
			break;

		case 6: // Receiver Line
			uart16550_handle_lsr(uart);
			break;

		default:
			printf("illegal IRQ status 0x%X\n", iir & 0xf);
			pc16c550_uart_update_notification(u);
			return -1;
			break;
		}
	}
	return 0;
}

static void pc16c550_uart_update_notification(struct rtuart * u)
{
	u8 v = 0;
	if (u->notify_mask & UART_NOTIFY_RXAVAILABLE)
		v |= 1;
	// if (u->notify_mask & UART_NOTIFY_TXEMPTY)

	if (u->notify_mask & UART_NOTIFY_TXREADY)
		v |= 2;
	
	if (u->notify_mask & UART_NOTIFY_LINEEVENT)
		v |= 4;
	if (u->notify_mask & UART_NOTIFY_INPUTCHANGED)
		v |= 8;
	rtuart_write_u8(u, PC16550_REG_IER, v);
	// printf ("update_notification(%02lX) -> %02X\n", u->notify_mask, v);
}

/* Notification will be automatically disabled once the notifier function is called. */
static int pc16c550_uart_enable_notification(struct rtuart * u, const unsigned long notify_mask)
{
	// may make this atomic.
	if ((u->notify_mask & notify_mask) != notify_mask)
	{
		u->notify_mask |= notify_mask;
		pc16c550_uart_update_notification(u);
	}
	// else printf ("pc16c550_uart_enable_notification nochange\n");
	return 0;
}

static int pc16c550_uart_disable_notification(struct rtuart * u, const unsigned long notify_mask)
{
	// may make this atomic.
	if ((u->notify_mask & notify_mask) != notify_mask)
	{
		u->notify_mask &= ~notify_mask;
		pc16c550_uart_update_notification(u);
	}
	// else printf ("pc16c550_uart_enable_notification nochange\n");
	return 0;
}

//===========================================================



struct rtuart_ops pc16c550_ops =
{
	.set_format = pc16c550_set_format,
	.get_format = pc16c550_get_format,
	.set_baudrate = pc16c550_set_baudrate,
	.get_baudrate = pc16c550_get_baudrate,
	.set_break = pc16c550_set_break,
	.get_break = pc16c550_get_break,
	.get_line_status = pc16c550_get_line_status,
	.set_modem_lines = pc16c550_set_modem_lines,
	.set_fifo_enable = pc16c550_set_fifo_enable,
	.flush_fifo = pc16c550_flush_fifo,
	.set_rxfifo_trigger_level = pc16c550_set_rxfifo_trigger_level,
	.set_txfifo_trigger_level = pc16c550_set_txfifo_trigger_level,
	.get_tx_fifo_size = pc16c550_get_tx_fifo_size,
	.get_rx_fifo_size = pc16c550_get_rx_fifo_size,
	.get_tx_fifo_free = pc16c550_get_tx_fifo_free,
	.get_rx_fifo_level = pc16c550_get_rx_fifo_level,
	.write_chars = pc16c550_write_chars,
	.read_chars = pc16c550_read_chars,
	.enable_notification = pc16c550_uart_enable_notification,
	.disable_notification = pc16c550_uart_disable_notification,
	.handle_irq = pc16c550_handle_irq
};

struct rtuart * pc16c550_create(const unsigned long base_clock)
{
	struct rtuart_pc16c550 * u = (struct rtuart_pc16c550 *)malloc(sizeof(struct rtuart_pc16c550));
	if (u)
	{
		u->uart.ops = &pc16c550_ops;
		u->fcr_shadow = 0;
		u->base_clock = base_clock;
	}
	return &u->uart;
}


//TODO: remove this once moved to kernel -- in the kernel use a bottomhalf that triggers itself or a timer.
void pc16c550_periodic_check(struct rtuart * u)
{
	uart16550_rxempty_task_handler(u);
}
