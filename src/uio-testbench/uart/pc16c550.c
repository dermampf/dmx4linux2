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
        struct tasklet_struct rxempty_tasklet;
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

        PC16550_LCR_DLAB   = (1<<7),
};


static inline u8 __read_register(struct rtuart *u, int regno)
{
  u8 v;
  rtuart_read_u8(u, regno, &v);
  return v;
}

static inline void __write_register(struct rtuart *u, int regno, u8 v)
{
  rtuart_write_u8(u, regno, v);
}

#define pc16c550_get_rbr(x)   __read_register(x,PC16550_REG_RBR)
#define pc16c550_set_thr(x,v) __write_register(x,PC16550_REG_THR,v)
#define pc16c550_get_ier(x)   __read_register(x,PC16550_REG_IER)
#define pc16c550_set_ier(x,v) __write_register(x,PC16550_REG_IER,v)
#define pc16c550_get_iir(x)   __read_register(x,PC16550_REG_IIR)
#define pc16c550_set_fcr(x,v) __write_register(x,PC16550_REG_FCR,v) // rtuart_write_u32(u,PC16550_REG_FCR,v)
#define pc16c550_get_lcr(x)   __read_register(x,PC16550_REG_LCR)
#define pc16c550_set_lcr(x,v) __write_register(x,PC16550_REG_LCR,v)
#define pc16c550_get_mcr(x)   __read_register(x,PC16550_REG_MCR)
#define pc16c550_set_mcr(x,v) __write_register(x,PC16550_REG_MCR,v)
#define pc16c550_get_lsr(x)   __read_register(x,PC16550_REG_LSR)
#define pc16c550_set_lsr(x,v) __write_register(x,PC16550_REG_LSR,v)
#define pc16c550_get_msr(x)   __read_register(x,PC16550_REG_MSR)
#define pc16c550_set_msr(x,v) __write_register(x,PC16550_REG_MSR,v)
#define pc16c550_get_scr(x)   __read_register(x,PC16550_REG_SCR)
#define pc16c550_set_scr(x,v) __write_register(x,PC16550_REG_SCR,v)
#define pc16c550_get_dll(x)   __read_register(x,PC16550_REG_DLL)
#define pc16c550_set_dll(x,v) __write_register(x,PC16550_REG_DLL,v)
#define pc16c550_get_dlm(x)   __read_register(x,PC16550_REG_DLM)
#define pc16c550_set_dlm(x,v) __write_register(x,PC16550_REG_LCR,v)



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
        u8 lcr = pc16c550_get_lcr(u);
	lcr &= ~0x3f;
	lcr |= databits-5;
	lcr |= (stopbits > 10) ? 4 : 0;
	lcr |= ( (parity=='e') ? 3 : (parity=='o') ? 1 : 0 ) << 3;
	pc16c550_set_lcr(u, lcr);
	return 0;
}

static int pc16c550_get_format(struct rtuart *u, int * databits, char * parity, int * stopbits)
{
        const u8 lcr = pc16c550_get_lcr(u);
	*databits = (lcr&3) + 5;
	*parity = (lcr & 8) ? ((lcr & 0x10) ? 'e' : 'o' ) : 'n';
	*stopbits = (lcr & 4) ? 20 : 10;
	return 0;
}

static void pc16c550_set_baudrate_divisor(struct rtuart * u, const unsigned int divisor)
{
	// lock uart
	const u8 lcr = pc16c550_get_lcr(u);
	pc16c550_set_lcr(u, lcr | PC16550_LCR_DLAB);
        pc16c550_set_dll(u,(u8)divisor);
        pc16c550_set_dlm(u,(u8)(divisor>>8));
	pc16c550_set_lcr(u,lcr);
	// unlock uart
}

static unsigned int pc16c550_get_baudrate_divisor(struct rtuart * u)
{
	// lock uart
	const u8 lcr = pc16c550_get_lcr(u);
	pc16c550_set_lcr(u, lcr | PC16550_LCR_DLAB);
        const unsigned int divisor = pc16c550_get_dll(u) | (pc16c550_get_dlm(u)<<8);
	pc16c550_set_lcr(u,lcr);
        return divisor;
	// unlock uart
}

static inline unsigned long pc16c550_baseclock(struct rtuart *u)
{
	struct rtuart_pc16c550 * uart = container_of(u, struct rtuart_pc16c550, uart);
        return uart->base_clock;
}

static inline unsigned long pc16c550_oversampling(struct rtuart * u)
{
        (void)u;
        return 16;
}

static int pc16c550_set_baudrate(struct rtuart *u, const unsigned int baudrate)
{
        if (baudrate <= 0)
                return -1;
        const unsigned int divisor = (pc16c550_baseclock(u) / pc16c550_oversampling(u)) / baudrate;
        pc16c550_set_baudrate_divisor(u, divisor);
	return 0;
}

static int pc16c550_get_baudrate(struct rtuart *u, unsigned int * baudrate)
{
        *baudrate = (pc16c550_baseclock(u) / pc16c550_oversampling(u)) / pc16c550_get_baudrate_divisor(u);
	return 0;
}

static int pc16c550_set_break(struct rtuart *u, const int on)
{
	u8 lcr = pc16c550_get_lcr(u);
	if (on)
		lcr |= 0x40;
	else
		lcr &= ~0x40;
	pc16c550_set_lcr(u, lcr);
	return 0;
}

static int pc16c550_get_break(struct rtuart *u, int *on)
{
	*on = (pc16c550_get_lcr(u) & 0x40) ? 1 : 0;
	return 0;
}

static int pc16c550_get_line_status(struct rtuart *u, unsigned int * line)
{
        *line = pc16c550_get_lsr(u);
	return 0;
}

static int pc16c550_set_modem_lines(struct rtuart *u,
				    unsigned int mask,
				    unsigned int line)
{
	// pc16c550_set_mcr(u, (pc16c550_get_mcr(u) & ~mask) | (line & mask));
	u8 mcr = pc16c550_get_mcr(u);
	mcr &= ~mask;
	mcr |= mask & line;
	// mcr = (mcr & ~mask) | (line & mask);
	// RTUART_OUTPUT_DTR, RTUART_OUTPUT_RTS, RTUART_OUTPUT_GENERIC0, RTUART_OUTPUT_GENERIC1
	pc16c550_set_mcr(u, mcr);
}

static u8 pc16c550_get_fcr(struct rtuart *u)
{
        struct rtuart_pc16c550 * uart = container_of(u, struct rtuart_pc16c550, uart);
        return uart->fcr_shadow;
}

static void pc16c550_rmw_fcr(struct rtuart *u, u8 mask, u8 value)
{
        struct rtuart_pc16c550 * uart = container_of(u, struct rtuart_pc16c550, uart);
        uart->fcr_shadow &= ~mask;
        uart->fcr_shadow |= mask & value &  ~6; // do not remember RX&TX FIFO Reset, otherwise we would accidentially reset FIFOS evertime we write fcr.
        pc16c550_set_fcr(u, uart->fcr_shadow | (mask & value));
}

static int pc16c550_set_fifo_enable(struct rtuart *u, const int on)
{
        // reset tx&rx fifo on enable
        pc16c550_rmw_fcr(u, 7, on ? 7 : 0);
	return 0;
}

static int pc16c550_get_fifo_enable(struct rtuart *u)
{
	struct rtuart_pc16c550 * uart = container_of(u, struct rtuart_pc16c550, uart);
        return (uart->fcr_shadow & 1);
}

static int pc16c550_flush_fifo (struct rtuart *u, const int flush_tx, const int flush_rx)
{
        pc16c550_rmw_fcr(u, 6, (flush_tx ? 4 : 0) | (flush_rx ? 2 : 0));
	return 0;
}

static int pc16c550_set_rxfifo_trigger_level (struct rtuart *u, const int level)
{
	switch (level)
	{
	case 14:
                pc16c550_rmw_fcr(u, 0xC0, 3<<6);
                break;
	case 8:
                pc16c550_rmw_fcr(u, 0xC0, 2<<6);
                break;
	case 4:
                pc16c550_rmw_fcr(u, 0xC0, 1<<6);
                break;
	case 1:
                pc16c550_rmw_fcr(u, 0xC0, 0<<6);
                break;
	default:
		return -1;
	}
	return 0;
}

static int pc16c550_get_rxfifo_trigger_level (struct rtuart *u)
{
        static const u8 levels[4] = { 1, 4, 8, 14 };
        const int fcr = pc16c550_get_fcr(u);
        return levels[fcr];
}

static int pc16c550_set_txfifo_trigger_level (struct rtuart *u, const int level)
{
	return -1;
}

// returns 1 if there is a THR and 0 if there is only the shift register.
static int pc16c550_get_tx_fifo_size(struct rtuart *u, unsigned int * level)
{
	(void)u;
        *level = pc16c550_get_fifo_enable(u) ? 16 : 1;
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
		pc16c550_set_thr(u, *(buffer++));
	return 0;
}

/*
 * Reads the number of octetes from the rx-fifo regardless if they are available or not.
 */
static int pc16c550_read_chars (struct rtuart *u, u8 * buffer, const size_t count)
{
	size_t c = count;
	while (c-- > 0)
		*(buffer++) = pc16c550_get_rbr(u);
	return count;
}


//============ IRQ Handler ================
static void pc16c550_uart_update_notification(struct rtuart * u);

static void uart16550_handle_lsr(struct rtuart_pc16c550 * uart)
{
	struct rtuart * u = &uart->uart;
        u8 lsr = pc16c550_get_lsr(u);

	if (lsr & PC16550_LSR_BREAK_INTERRUPT) // break interrupt - we also receive a 0-char -> dummy read.
		pc16c550_get_rbr(u);

	// PC16550_LSR_DATA_READY
	// PC16550_LSR_ERROR_IN_RXFIFO

        const unsigned int lineStatusEvent = uart16550_lsr_to_event(lsr);
	if (lineStatusEvent && u->client && u->client->callbacks && u->client->callbacks->line_status_event)
	{
		u->client->callbacks->line_status_event(u, lineStatusEvent);
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
#if 0
                                tasklet_schedule( &uart->rxempty_tasklet );
#else
				// Linux: schedule_delayed_task(uart->rxempty_task);

                                while (0==(pc16c550_get_lsr(u) & PC16550_LSR_TRANSMITTER_EMPTY))
					usleep(88);
				u->client->callbacks->transmitter_empty(u);
#endif
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

static void uart16550_rxempty_function( unsigned long data )
{
        uart16550_rxempty_task_handler((struct rtuart *)data);
}

// may have interrup handler in a seperate file
static int pc16c550_handle_irq(struct rtuart *u)
{
	struct rtuart_pc16c550 * uart = container_of(u, struct rtuart_pc16c550, uart);
	while (1)
	{
                const u8 iir = pc16c550_get_iir(u);
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
                        {
                                const u8 msr = pc16c550_get_msr(u);
                                if (u->client && u->client->callbacks && u->client->callbacks->modem_input_changed)
                                        u->client->callbacks->modem_input_changed(u, (msr>>4)&15, msr&15);
                        }
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
	pc16c550_set_ier(u, v);
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
                u->uart.address_offset = 0;
                u->uart.irqno = 0;
		u->fcr_shadow = 0;
		u->base_clock = base_clock;
                tasklet_init(&u->rxempty_tasklet, uart16550_rxempty_function, (unsigned long)u);
	}
	return &u->uart;
}

struct rtuart * pc16c550_create_at(const unsigned long base_clock,
                                   unsigned long address_offset,
                                   int irqno)
{
        struct rtuart * u = pc16c550_create(base_clock);
        if (u)
        {
                u->address_offset = address_offset;
                u->irqno = irqno;
        }
        return u;
}

//TODO: remove this once moved to kernel -- in the kernel use a bottomhalf that triggers itself or a timer.
void pc16c550_periodic_check(struct rtuart * u)
{
	uart16550_rxempty_task_handler(u);
}
