#include "rtuart.h"
#include "rtuart_priv.h"
#include "rtuart_values.h"
#include "rtuart_pc16c550.h"
#include <stdlib.h>
#include <stdio.h>

#include <kernel.h>

struct rtuart_pc16c550 {
	struct rtuart uart;
	u32 base_clock;
	struct rtuart_buffer * tx_buffer;
	struct rtuart_buffer * rx_buffer;
	int in_irq; // make it an atomic. increment every time we enter an irq and decrement on exit.
	u8  shadow_fcr;
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

// PC16C550_RBR_*
// PC16C550_THR_*

enum
{
	PC16C550_IER_RECEIVED_DATA_AVAILABLE = (1<<0),
	PC16C550_IER_TRANSMITTER_HOLDING_REGISTER_EMPTY = (1<<1),
	PC16C550_IER_RECEIVER_LINE_STATUS = (1<<2),
	PC16C550_IER_MODEM_STATUS = (1<<3)
};

enum {
	PC16C550_IIR_IRQ_NONE = 1,
	PC16C550_IIR_IRQ_RECEIVER_LINE_STATUS = 6,
	PC16C550_IIR_IRQ_RECEIVED_DATA_AVAILABLE = 4,
	PC16C550_IIR_IRQ_CHARACTER_TIMEOUT = 0xC,
	PC16C550_IIR_IRQ_TRANSMITTER_HOLDING_EMPTY = 2,
	PC16C550_IIR_IRQ_MODEM_STATUS = 0,
	PC16C550_IIR_IRQ_MASK = (0xf),
};

enum {
	PC16C550_FCR_FIFO_ENABLE = (1<<0),
	PC16C550_FCR_RCVR_FIFO_RESET = (1<<1),
	PC16C550_FCR_XMIT_FIFO_RESET = (1<<2),
	PC16C550_FCR_DMA_MODE0 = (0<<3),
	PC16C550_FCR_DMA_MODE1 = (1<<3),
	PC16C550_FCR_RCVR_TRIGGER_1BYTE  = (0<<6),
	PC16C550_FCR_RCVR_TRIGGER_4BYTE  = (1<<6),
	PC16C550_FCR_RCVR_TRIGGER_8BYTE  = (2<<6),
	PC16C550_FCR_RCVR_TRIGGER_14BYTE = (3<<6),
};
static inline int PC16C550_FCR_RCVR_TRIGGER(int level) {
	switch(level)
	{
	case 1:  return PC16C550_FCR_RCVR_TRIGGER_1BYTE;
	case 4:  return PC16C550_FCR_RCVR_TRIGGER_4BYTE;
	case 8:  return PC16C550_FCR_RCVR_TRIGGER_8BYTE;
	case 14: return PC16C550_FCR_RCVR_TRIGGER_14BYTE;
	default: break;
	}
	return -1;
}

enum {
	PC16550_LCR_WLS0 = (1<<0),
	PC16550_LCR_WLS1 = (1<<1),
	PC16550_LCR_STB = (1<<2),
	PC16550_LCR_PEN = (1<<3),
	PC16550_LCR_EPS = (1<<4),
	PC16550_LCR_STICK_PARITY = (1<<5),
	PC16550_LCR_BREAK = (1<<6),
	PC16550_LCR_DLAB  = (1<<7),
};

enum {
	PC16550_MCR_DTR = (1<<0),
	PC16550_MCR_RTS = (1<<1),
	PC16550_MCR_OUT1 = (1<<2),
	PC16550_MCR_OUT2 = (1<<3),
	PC16550_MCR_LOOP = (1<<3)
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

enum {
	PC16550_MSR_DCTS = (1<<0), /* Delta CTS - CTS has changed since last read */
	PC16550_MSR_DDSR = (1<<1), /* Delta DSR - DSR has changed since last read  */
	PC16550_MSR_TERI = (1<<2), /* Trailing Edge Ring Indicator - RI change from low to high */
	PC16550_MSR_DDCD = (1<<3), /* Delta DCD - DCD has changed since last read */
	PC16550_MSR_CTS  = (1<<4), /* CTS value */
	PC16550_MSR_DSR  = (1<<5), /* DSR value */
	PC16550_MSR_RI   = (1<<6), /* RI  value */
	PC16550_MSR_DCD  = (1<<7), /* CDC value */
};

// PC16550_SCR_*
// PC16550_DLL_*
// PC16550_DLM_*



static void pc16c550_tx_stop (struct rtuart * uart, struct rtuart_buffer * buffer, int * allready_written);
static int  pc16c550_tx_written (struct rtuart * uart, struct rtuart_buffer * buffer);
static long pc16c550_rx_stop (struct rtuart * uart, struct rtuart_buffer * buffer);


static void _pc16550_flush_rx_fifo(struct rtuart_pc16c550 * pc16c550)
{
	rtuart_write_u8(&pc16c550->uart, PC16550_REG_FCR, pc16c550->shadow_fcr | PC16C550_FCR_RCVR_FIFO_RESET);
	// rtuart_write_u8(uart, PC16550_REG_FCR, pc16c550->shadow_fcr | PC16C550_FCR_XMIT_FIFO_RESET);
}

static void _pc16550_enable_fifo(struct rtuart_pc16c550 * pc16c550)
{
	pc16c550->shadow_fcr |= PC16C550_FCR_FIFO_ENABLE;
	rtuart_write_u8(&pc16c550->uart, PC16550_REG_FCR, pc16c550->shadow_fcr);
}

static int buffer_is_complete(struct rtuart_buffer * buffer)
{
	return !buffer || (buffer->transfered >= buffer->size) || (buffer->transfered >= buffer->validcount);
}

static void pc16c550_destroy   (struct rtuart * uart)
{
	printk("pc16c550_destroy\n");
	if (uart)
		free(uart);
}

static void pc16c550_set_baudrate  (struct rtuart * uart, const unsigned long baudrate)
{
	struct rtuart_pc16c550 * pc16c550 = container_of(uart, struct rtuart_pc16c550, uart);
	// lock uart
	const int oversampling = 16;
        const unsigned int divisor = (pc16c550->base_clock / oversampling) / baudrate;
	printk("pc16c550_set_baudrate(%lu) : base-clock:%lu divisor:%d\n",
	       baudrate, pc16c550->base_clock, divisor);
	u8 lcr;
	rtuart_read_u8 (uart, PC16550_REG_LCR, &lcr);
	rtuart_write_u8(uart, PC16550_REG_LCR, lcr | PC16550_LCR_DLAB);
	rtuart_write_u8(uart, PC16550_REG_DLL, (u8)divisor);
	rtuart_write_u8(uart, PC16550_REG_DLM, (u8)(divisor>>8));
	rtuart_write_u8(uart, PC16550_REG_LCR, lcr);
	// unlock uart
}

static void pc16c550_get_baudrate  (struct rtuart * uart, unsigned long * baudrate)
{
	printk("pc16c550_get_baudrate()\n");
	if (uart && baudrate) {
		struct rtuart_pc16c550 * pc16c550 = container_of(uart, struct rtuart_pc16c550, uart);
		// lock uart
		u8 divisor_msb, divisor_lsb;
		u8 lcr;
		const int oversampling = 16;
		rtuart_read_u8 (uart, PC16550_REG_LCR, &lcr);
		rtuart_write_u8(uart, PC16550_REG_LCR, lcr | PC16550_LCR_DLAB);
		rtuart_read_u8 (uart, PC16550_REG_DLL, &divisor_lsb);
		rtuart_read_u8 (uart, PC16550_REG_DLM, &divisor_msb);
		rtuart_write_u8(uart, PC16550_REG_LCR, lcr);
		*baudrate = (pc16c550->base_clock / oversampling) / ((divisor_msb<<8) + divisor_lsb);
		// unlock uart
		// return 0;
	}
	// return 1;
}

static void pc16c550_set_format    (struct rtuart * uart, const int databits, const char parity, const char stopbits)
{
	printk("pc16c550_set_format(databits:%d, parity:%c, stopbits:%d)\n", databits, parity, stopbits);
	u8 lcr;
	rtuart_read_u8(uart, PC16550_REG_LCR, &lcr);
	lcr &= ~(PC16550_LCR_WLS0 |
		 PC16550_LCR_WLS1 |
		 PC16550_LCR_STB |
		 PC16550_LCR_PEN |
		 PC16550_LCR_EPS |
		 PC16550_LCR_STICK_PARITY );

	static char bits2code[4] =
		{
			0,
			PC16550_LCR_WLS0,
			PC16550_LCR_WLS1,
			PC16550_LCR_WLS0|PC16550_LCR_WLS1
		};

	lcr |=  bits2code[(databits-5)&3];
	lcr |=  (stopbits > 1) ? PC16550_LCR_STB : 0;
	lcr |=  (parity=='e') ? (PC16550_LCR_PEN|PC16550_LCR_EPS) :
		(parity=='o') ? PC16550_LCR_PEN :
		(parity=='1') ? (PC16550_LCR_STICK_PARITY|PC16550_LCR_PEN) :
		(parity=='0') ? (PC16550_LCR_STICK_PARITY|PC16550_LCR_PEN|PC16550_LCR_EPS) :
		0;
	rtuart_write_u8(uart, PC16550_REG_LCR, lcr);
}

// unused
static int pc16c550_get_format(struct rtuart *uart, int * databits, char * parity, int * stopbits)
{
	u8 lcr;
	if (rtuart_read_u8(uart, PC16550_REG_LCR, &lcr))
		return -1;
	*databits = (lcr&3) + 5;
	switch(lcr&(PC16550_LCR_STICK_PARITY|PC16550_LCR_PEN|PC16550_LCR_EPS))
	{
	case PC16550_LCR_STICK_PARITY|PC16550_LCR_PEN|PC16550_LCR_EPS:
		*parity = '0';
		break;
	case PC16550_LCR_STICK_PARITY|PC16550_LCR_PEN:
		*parity = '1';
		break;
	case PC16550_LCR_PEN|PC16550_LCR_EPS:
		*parity = 'e';
		break;
	case PC16550_LCR_PEN:
		*parity = 'o';
		break;
	default:
		*parity = 'n';
		break;
	}
	*stopbits = (lcr & PC16550_LCR_STB) ? 2 : 1;
	return 0;
}

static void pc16c550_set_handshake (struct rtuart * uart, rtuart_handshake_t hs)
{
	(void)uart;
	printk("pc16c550_set_handshake(%d) - !! NOT YET IMPLEMENTED\n", hs);

	switch(hs)
	{
	case  RTUART_HANDSHAKE_OFF:
		break;
	case RTUART_HANDSHAKE_RTSCTS:
		break;
	case RTUART_HANDSHAKE_DSRDTR:
		break;
	case RTUART_HANDSHAKE_RTSCTS_DSRDTR:
		break;
	default:
		break;
	}
}

static void pc16c550_set_control (struct rtuart * uart, const unsigned long control_mask)
{
	printk("pc16c550_set_control(%08lX)\n", control_mask);

	if (control_mask & RTUART_CONTROL_BREAK) {
		u8 old_lcr;
		if (!rtuart_read_u8(uart, PC16550_REG_LCR, &old_lcr)) {
			u8 lcr = old_lcr;
			
			if (control_mask & RTUART_CONTROL_BREAK)
				lcr |= PC16550_LCR_BREAK;
			
			if (lcr != old_lcr)
				rtuart_write_u8(uart, PC16550_REG_LCR, lcr);
		}
	}

	if (control_mask & (RTUART_CONTROL_RTS|RTUART_CONTROL_DTR|RTUART_CONTROL_OUT1|RTUART_CONTROL_OUT2|RTUART_CONTROL_LOOP)) {
		u8 old_mcr;
		if (!rtuart_read_u8(uart, PC16550_REG_MCR, &old_mcr)) {
			u8 mcr = old_mcr;

			if (control_mask & RTUART_CONTROL_RTS)
				mcr |= PC16550_MCR_RTS;

			if (control_mask & RTUART_CONTROL_DTR)
				mcr |= PC16550_MCR_DTR;

			if (control_mask & RTUART_CONTROL_OUT1)
				mcr |= PC16550_MCR_OUT1;

			if (control_mask & RTUART_CONTROL_OUT2)
				mcr |= PC16550_MCR_OUT2;

			if (control_mask & RTUART_CONTROL_LOOP)
				mcr |= PC16550_MCR_LOOP;

			if (mcr != old_mcr)
				rtuart_write_u8(uart, PC16550_REG_MCR, mcr);
		}
	}
}

static void pc16c550_clr_control (struct rtuart * uart, const unsigned long control_mask)
{
	printk("pc16c550_clr_control(%08lX)\n", control_mask);

	if (control_mask & RTUART_CONTROL_BREAK) {
		u8 old_lcr;
		if (!rtuart_read_u8(uart, PC16550_REG_LCR, &old_lcr)) {
			u8 lcr = old_lcr;

			if (control_mask & RTUART_CONTROL_BREAK)
				lcr &= ~PC16550_LCR_BREAK;

			if (lcr != old_lcr)
				rtuart_write_u8(uart, PC16550_REG_LCR, lcr);
		}
	}

	if (control_mask & (RTUART_CONTROL_RTS|RTUART_CONTROL_DTR|RTUART_CONTROL_OUT1|RTUART_CONTROL_OUT2|RTUART_CONTROL_LOOP)) {
		u8 old_mcr;
		if (!rtuart_read_u8(uart, PC16550_REG_MCR, &old_mcr)) {
			u8 mcr = old_mcr;

			if (control_mask & RTUART_CONTROL_RTS)
				mcr &= ~PC16550_MCR_RTS;

			if (control_mask & RTUART_CONTROL_DTR)
				mcr &= ~PC16550_MCR_DTR;

			if (control_mask & RTUART_CONTROL_OUT1)
				mcr &= ~PC16550_MCR_OUT1;

			if (control_mask & RTUART_CONTROL_OUT2)
				mcr &= ~PC16550_MCR_OUT2;

			if (control_mask & RTUART_CONTROL_LOOP)
				mcr &= ~PC16550_MCR_LOOP;

			if (mcr != old_mcr)
				rtuart_write_u8(uart, PC16550_REG_MCR, mcr);
		}
	}
}

static void pc16c550_get_control (struct rtuart * uart, unsigned long * control)
{
	printk("pc16c550_get_control()\n");

	u8 lcr;
	u8 mcr;

	*control = 0;

	if (!rtuart_read_u8(uart, PC16550_REG_LCR, &lcr)) {
		if (lcr & PC16550_LCR_BREAK)
			*control |= RTUART_CONTROL_BREAK;
	}

	if (!rtuart_read_u8(uart, PC16550_REG_MCR, &mcr)) {
		if (mcr & PC16550_MCR_RTS)
			*control |= RTUART_CONTROL_RTS;

		if (mcr & PC16550_MCR_DTR)
			*control |= RTUART_CONTROL_DTR;

		if (mcr & PC16550_MCR_OUT1)
			*control |= RTUART_CONTROL_OUT1;

		if (mcr & PC16550_MCR_OUT2)
			*control |= RTUART_CONTROL_OUT2;

		if (mcr & PC16550_MCR_LOOP)
			*control |= RTUART_CONTROL_LOOP;
	}
}


static void pc16c550_update_notification(struct rtuart * uart)
{
	u8 ier = 0;
	if (uart->notify_mask & RTUART_NOTIFY_TRANSMITTER_READY)
		ier |= PC16C550_IER_TRANSMITTER_HOLDING_REGISTER_EMPTY;

	if (uart->notify_mask & RTUART_NOTIFY_TRANSMITTER_EMPTY)
		ier |= PC16C550_IER_TRANSMITTER_HOLDING_REGISTER_EMPTY;

	if (uart->notify_mask & RTUART_NOTIFY_RECEIVER_HASDATA)
		ier |= PC16C550_IER_RECEIVED_DATA_AVAILABLE;

	if (uart->notify_mask & RTUART_NOTIFY_RECEIVER_TIMEOUT)
		ier |= PC16C550_IER_RECEIVED_DATA_AVAILABLE;

	if (uart->notify_mask & RTUART_NOTIFY_RECEIVER_EVENT) {
		/* Errors, Break, CTS-change, ... */
		ier |= PC16C550_IER_RECEIVER_LINE_STATUS;
		ier |= PC16C550_IER_MODEM_STATUS;
	}

	printk(KERN_INFO"pc16c550_update_notification():notify:%08lX  ier:%02X\n",
	       uart->notify_mask,
	       ier);

	rtuart_write_u8(uart, PC16550_REG_IER, ier);
}

static void pc16c550_set_notify (struct rtuart * uart, const unsigned long notify)
{
	struct rtuart_pc16c550 * pc16c550 = container_of(uart, struct rtuart_pc16c550, uart);
	printk(KERN_INFO"pc16c550_set_notify(%08lX) old:%08lX irq:%d\n", notify, uart->notify_mask, pc16c550->in_irq);
	unsigned long new_notify = uart->notify_mask | notify;
	if (uart->notify_mask != new_notify) {
		uart->notify_mask = new_notify;
		if (0 == pc16c550->in_irq)
			pc16c550_update_notification(uart);
	}
}

static void pc16c550_clr_notify (struct rtuart * uart, const unsigned long notify)
{
	struct rtuart_pc16c550 * pc16c550 = container_of(uart, struct rtuart_pc16c550, uart);
	printk(KERN_INFO"pc16c550_clr_notify(%08lX)\n", notify);
	unsigned long new_notify = uart->notify_mask & ~notify;
	if (uart->notify_mask != new_notify) {
		uart->notify_mask = new_notify;
		if (!pc16c550->in_irq)
			pc16c550_update_notification(uart);
	}
}

static void pc16c550_get_notify (struct rtuart * uart, unsigned long  * notify_mask)
{
	printk(KERN_INFO"pc16c550_get_notify()\n");
	*notify_mask = uart->notify_mask;
}

static void pc16c550_tx_start (struct rtuart * uart, struct rtuart_buffer * buffer)
{
	printk (KERN_INFO"pc16c550_tx_start(buffer:%p)\n", buffer);

	struct rtuart_pc16c550 * pc16c550 = container_of(uart, struct rtuart_pc16c550, uart);
	buffer->transfered = 0;
	if (pc16c550->tx_buffer) {
		int allready_written = 0;
		pc16c550_tx_stop (uart, buffer, &allready_written);
	}
	if (!pc16c550->tx_buffer) {
		pc16c550->tx_buffer = buffer;
		pc16c550_set_notify (uart, RTUART_NOTIFY_TRANSMITTER_READY);
	}
}

static void pc16c550_tx_stop (struct rtuart * uart, struct rtuart_buffer * buffer, int * allready_written)
{
	struct rtuart_pc16c550 * pc16c550 = container_of(uart, struct rtuart_pc16c550, uart);
	printk("pc16c550_tx_stop(buffer:%p, allready_written:%d)\n",
	       buffer,
	       allready_written ? *allready_written : -1);

	if (pc16c550->tx_buffer == buffer) {
		if (allready_written)
			*allready_written = pc16c550_tx_written (uart, pc16c550->tx_buffer);
		pc16c550->tx_buffer = 0;
	}
}

static int  pc16c550_tx_written (struct rtuart * uart, struct rtuart_buffer * buffer)
{
	(void)uart;
	printk("pc16c550_tx_written(buffer:%p)\n", buffer);
	return buffer->transfered;
}

static void pc16c550_rx_start (struct rtuart * uart, struct rtuart_buffer * buffer)
{
	printk("pc16c550_rx_start(buffer:%p)\n", buffer);

	struct rtuart_pc16c550 * pc16c550 = container_of(uart, struct rtuart_pc16c550, uart);
	if (pc16c550->rx_buffer) {
		pc16c550_rx_stop (uart, pc16c550->rx_buffer);
	}

	buffer->transfered = 0;
	pc16c550->rx_buffer = buffer;
	pc16c550_set_notify (uart, RTUART_NOTIFY_RECEIVER_HASDATA|RTUART_NOTIFY_RECEIVER_TIMEOUT);
}

static long pc16c550_rx_stop (struct rtuart * uart, struct rtuart_buffer * buffer)
{
	struct rtuart_pc16c550 * pc16c550 = container_of(uart, struct rtuart_pc16c550, uart);
	printk("pc16c550_rx_stop\n");
	pc16c550->rx_buffer = 0;
	pc16c550_clr_notify (uart, RTUART_NOTIFY_RECEIVER_HASDATA|RTUART_NOTIFY_RECEIVER_TIMEOUT);
	return 0;
}

static void pc16c550_rx_set_timeout(struct rtuart * uart, struct timespec * timeout)
{
	(void)uart;
	printk("pc16c550_rx_set_timeout\n");
}


static int copy_from_buffer_to_transmitter(struct rtuart_buffer * buffer,
					   struct rtuart_pc16c550 * pc16c550,
					   const int count)
{
	int copied = 0;
	while ((copied < count) && (copied < 8)) {
		printk (KERN_INFO"[] copied:%d  count:%d  transfered:%d  valid:%d  size:%d\n",
			copied,
			count,
			buffer->transfered,
			buffer->validcount,
			buffer->size);

		if ((buffer->transfered >= buffer->validcount) ||
		    (buffer->transfered >= buffer->size))
			break;
		const u8 c = buffer->data[buffer->transfered++];
		rtuart_write_u8(&pc16c550->uart, PC16550_REG_THR, c);
		++copied;
	}
	return copied;
}


static void handle_received_data(struct rtuart_pc16c550 * pc16c550,
				 struct rtuart_buffer * buffer,
				 const int due_to_timeout)
{
	struct rtuart * uart = &pc16c550->uart;

	if (!buffer ||
	    (buffer->transfered >= buffer->validcount) ||
	    (buffer->transfered >= buffer->size)) {
		if (uart->cb->rx_char) {
			u8 c;
			rtuart_read_u8(uart, PC16550_REG_RBR, &c);
			uart->notify_mask &= ~RTUART_NOTIFY_RECEIVER_HASDATA;
			rtuart_call_rx_char (uart, c);
		}
		else
			_pc16550_flush_rx_fifo (pc16c550);
	}
	else {
		rtuart_read_u8(uart, PC16550_REG_RBR, buffer->data + buffer->transfered);
		++buffer->transfered;

		//-- handle trigger.
		if (buffer->triggermask) {
			unsigned long triggermask = 0;
			int i;
			for (i = 0; i < RTUART_MAX_TRIGGERS; ++i) {
				if ((buffer->triggermask & (1<<i)) &&
				    (buffer->transfered > buffer->trigger[i]))
					triggermask |= (1<<i);
			}
			if (triggermask) {
				rtuart_call_rx_trigger(uart,
							buffer,
							triggermask);
				buffer->triggermask &= ~triggermask;
			}
		}

		//-- handle other callbacks.
		if ((buffer->transfered >= buffer->validcount) ||
		    (buffer->transfered >= buffer->size)) {
			pc16c550->rx_buffer = 0;
			uart->notify_mask &= ~RTUART_NOTIFY_RECEIVER_HASDATA;
			rtuart_call_rx_end (uart, buffer);
		} else if (due_to_timeout)
			rtuart_call_rx_timeout (uart, buffer);
	}
}

static unsigned int uart16c550_msr_to_event(const u8 msr)
{
	return
		((msr & PC16550_MSR_DCTS) ? RTUART_EVENT_CTS_CHANGE : 0) |
		((msr & PC16550_MSR_DDSR) ? RTUART_EVENT_DSR_CHANGE : 0) |
		((msr & PC16550_MSR_TERI) ? RTUART_EVENT_RI_CHANGE : 0) |
		((msr & PC16550_MSR_DDCD) ? RTUART_EVENT_DCD_CHANGE : 0);
}

static unsigned int uart16c550_lsr_to_event(const u8 lsr)
{
	// later use uint64_t and use   return (LsrToEvent >> ((lsr>>1)&15)) & 15;
	// or use array of 16 uint8_t and do return LsrToEvent[(lsr>>1)&15];

	return  ((lsr & PC16550_LSR_OVERRUN_ERROR) ? RTUART_EVENT_OVERRUN_ERROR : 0) |
		((lsr & PC16550_LSR_PARITY_ERROR)  ? RTUART_EVENT_PARITY_ERROR : 0) |
		((lsr & PC16550_LSR_FRAMING_ERROR) ? RTUART_EVENT_FRAME_ERROR : 0) |
		((lsr & PC16550_LSR_BREAK_INTERRUPT) ? RTUART_EVENT_BREAK : 0)
		;
}

static void update_iir_and_lsr(struct rtuart * uart, u8 * iir, u8 * lsr)
{
	rtuart_read_u8(uart, PC16550_REG_IIR, iir);

	switch ((*iir) & PC16C550_IIR_IRQ_MASK)
	{
	case PC16C550_IIR_IRQ_RECEIVED_DATA_AVAILABLE:
	case PC16C550_IIR_IRQ_CHARACTER_TIMEOUT:
	case PC16C550_IIR_IRQ_NONE:
	case PC16C550_IIR_IRQ_RECEIVER_LINE_STATUS:
		rtuart_read_u8(uart, PC16550_REG_LSR, lsr);
		break;
	default:
		break;
	}
}

static void pc16c550_handle_irq (struct rtuart * uart)
{
	u8 iir, lsr;
	struct rtuart_pc16c550 * pc16c550 = container_of(uart, struct rtuart_pc16c550, uart);
	pc16c550->in_irq = 1;
	printk("pc16c550_handle_irq\n");
	update_iir_and_lsr(uart, &iir, &lsr);
	while (1)
	{
		printk (KERN_DEBUG"iir:%02X  cause:%02X\n", iir, iir & PC16C550_IIR_IRQ_MASK);
		switch (iir & PC16C550_IIR_IRQ_MASK)
		{
		case PC16C550_IIR_IRQ_RECEIVED_DATA_AVAILABLE:
		case PC16C550_IIR_IRQ_CHARACTER_TIMEOUT:
		        printk (KERN_DEBUG"ENTER_RECV\n");
			if (uart->notify_mask & RTUART_NOTIFY_RECEIVER_HASDATA)
		        {
				// do
				while (1)
				{
					const int cause_is_timeout =
						(iir & PC16C550_IIR_IRQ_MASK)==PC16C550_IIR_IRQ_CHARACTER_TIMEOUT;
					handle_received_data(
						pc16c550,
						pc16c550->rx_buffer,
						cause_is_timeout);

					update_iir_and_lsr(uart, &iir, &lsr);

					int cont = 0;
					if ((iir & PC16C550_IIR_IRQ_MASK) == PC16C550_IIR_IRQ_RECEIVED_DATA_AVAILABLE)
						cont = 1;
					if ((iir & PC16C550_IIR_IRQ_MASK) == PC16C550_IIR_IRQ_CHARACTER_TIMEOUT)
						cont = 1;
					if (lsr & PC16550_LSR_DATA_READY)
						cont = 1;
					if (!cont)
						break;
				}
				printk (KERN_DEBUG"LEAVE_RECV(1) iir:%02X  lsr:%02X\n", iir, lsr);
				continue; // we allready read the IIR and LSR.
			}
			else
				_pc16550_flush_rx_fifo (pc16c550);
			printk (KERN_DEBUG"LEAVE_RECV(2)\n");
			break;

		case PC16C550_IIR_IRQ_TRANSMITTER_HOLDING_EMPTY:
			/*
			 * The THRE interrupt is triggered once the transmitter is empty.
			 * And either data is written to the THR and the THRE interrupt
			 * is emitted later or no data is written anymore and the THR
			 * stays empty and we need to wayt for the Transmitter to become
			 * empty. With FIFO's enabled the THRE means that the FIFO is
			 * completely empty but the transmitter is still sending. At
			 * 250KBaud we have 44us to fill up the buffer until the line
			 * becomes idle (only just a waste line bandwidth).
			 */
		        {
				int available_space = 16; // available space in tx fifo.
				printk (KERN_INFO"irq:THRE  notify:%X\n", uart->notify_mask);
				if (uart->notify_mask & RTUART_NOTIFY_TRANSMITTER_READY)
				{
					int copied = 0;
					do
					{
						if (pc16c550->tx_buffer && (0 == pc16c550->tx_buffer->transfered))
							rtuart_call_tx_start (uart, pc16c550->tx_buffer);

						copied = copy_from_buffer_to_transmitter(pc16c550->tx_buffer,
											 pc16c550,
											 available_space);
						if (copied > 0)
							available_space -= copied;

						if (buffer_is_complete(pc16c550->tx_buffer)) {
							uart->notify_mask &= ~RTUART_NOTIFY_TRANSMITTER_READY;

							struct rtuart_buffer * b = pc16c550->tx_buffer;
							pc16c550->tx_buffer = 0;
							rtuart_call_tx_end1(uart, b);

							if ((pc16c550->tx_buffer == 0) &&
							    (uart->notify_mask & RTUART_NOTIFY_TRANSMITTER_EMPTY)) {
								pc16c550->tx_buffer = b;
							}
						}
					} while ((copied > 0) && (available_space > 0) &&
						 (uart->notify_mask & RTUART_NOTIFY_TRANSMITTER_READY) &&
						 pc16c550->tx_buffer && (pc16c550->tx_buffer->transfered==0)
						);
				}
				printk (KERN_INFO"irq:THRE(2) notify:%X\n", uart->notify_mask);
			}
			break;

		case PC16C550_IIR_IRQ_MODEM_STATUS:
		        {
				u8 msr;
				if (!rtuart_read_u8(uart, PC16550_REG_MSR, &msr)) {
					const int event = uart16c550_msr_to_event(msr);
					if (event)
						rtuart_call_input_change (&pc16c550->uart,
									  event,
									  uart16c550_msr_to_event(msr>>4));
				}
			}
			break;

		case PC16C550_IIR_IRQ_NONE:
			printk (KERN_INFO"IDLE\n");
			if ((uart->notify_mask & RTUART_NOTIFY_TRANSMITTER_EMPTY) &&
			    buffer_is_complete(pc16c550->tx_buffer)) {
				if (lsr & PC16550_LSR_TRANSMITTER_EMPTY) {
					printk (KERN_INFO"Transmitter Is Empty\n");
					uart->notify_mask &= ~RTUART_NOTIFY_TRANSMITTER_EMPTY;
					struct rtuart_buffer * b = pc16c550->tx_buffer;
					pc16c550->tx_buffer = 0; // TODO: buffer gets lost - memory leak

					printk (KERN_INFO"call rtuart_call_tx_end2\n");
					rtuart_call_tx_end2(uart, b);
					if (pc16c550->tx_buffer) {
						//-- the following call forces a THRE interrupt to be generated
						//-- if the TRANSMITTER_READY has been enabled and the THRE
						//-- is allready enabled.
						//-- There is always an interrupt generated on a low to high transition
						//-- of the "Enable Transmitter Holding Register Empty Interrupt" bit.
						u8 ier;
						if (!rtuart_read_u8(uart, PC16550_REG_IER, &ier)) {
							ier &= ~PC16C550_IER_TRANSMITTER_HOLDING_REGISTER_EMPTY;
							rtuart_write_u8(uart, PC16550_REG_IER, ier);
						}
					}
					
				}
				else if (lsr & PC16550_LSR_THRE) {
					// Transmitter is about to become empty.
					// so wait for some more time (one more loop).
					printk (KERN_INFO"Transmitter Is NOT YET Empty\n");
					break;
				}
			}
			pc16c550_update_notification(uart);
			pc16c550->in_irq = 0;
			return;

			// Do not return, if we wait for a transmitter empty and
			// the thr is empty. It can not last for so long (44us at dmx).
			// This way we do not lose e.g. break or parity errors,
			// that are read from the same register (LSR) as the
			// transmitter empty flag (bad design).
			// For slower baudrates (e.g. 60 baud) this can be a very long time.

		// Overrun, Parity, Framing-Errors or Break Interrupt.
		case PC16C550_IIR_IRQ_RECEIVER_LINE_STATUS:
			/* We receive a 0-char on a break interupt
			 * We just do a dummy read.
			 * This behavior may need to be expanded
			 * to Frame and Parity errors.
			 */
			if (lsr & PC16550_LSR_BREAK_INTERRUPT) {
				u8 dummy;
				rtuart_read_u8(uart, PC16550_REG_RBR, &dummy);
			}
			{
				const int event = uart16c550_lsr_to_event(lsr);
				if (event)
					uart->cb->rx_err (uart, event);
			}
			break;

		default:
			printk (KERN_ERR"illegal IRQ status 0x%X\n", iir & PC16C550_IIR_IRQ_MASK);
			pc16c550_update_notification(uart);
			pc16c550->in_irq = 0;
			return; // with error
			break;
		}
		update_iir_and_lsr(uart, &iir, &lsr);
	}
}



static struct rtuart_ops rtuart_pc16c550_ops =
{
	.destroy       = pc16c550_destroy,
	.set_baudrate  = pc16c550_set_baudrate,
	.get_baudrate  = pc16c550_get_baudrate,
	.set_format    = pc16c550_set_format,
	.set_handshake = pc16c550_set_handshake,
	.set_control   = pc16c550_set_control,
	.clr_control   = pc16c550_clr_control,
	.get_control   = pc16c550_get_control,
	.set_notify    = pc16c550_set_notify,
	.clr_notify    = pc16c550_clr_notify,
	.get_notify    = pc16c550_get_notify,
	.tx_start      = pc16c550_tx_start,
	.tx_stop       = pc16c550_tx_stop,
	.tx_written    = pc16c550_tx_written,
	.rx_start      = pc16c550_rx_start,
	.rx_stop       = pc16c550_rx_stop,
	.rx_set_timeout = pc16c550_rx_set_timeout,
	.handle_irq    = pc16c550_handle_irq,
};

struct rtuart * rtuart_pc16c550_create(struct rtuart_bus * bus,
				       const unsigned long base_clock_hz)
{
	struct rtuart_pc16c550 * pc16c550 = malloc(sizeof(struct rtuart_pc16c550));
	if (pc16c550) {
		rtuart_init(&pc16c550->uart);
		pc16c550->uart.bus = bus;
		pc16c550->base_clock = base_clock_hz;
		pc16c550->uart.ops = &rtuart_pc16c550_ops;
		pc16c550->in_irq = 0;

		pc16c550_update_notification(&pc16c550->uart);

		pc16c550->shadow_fcr = 0;
		pc16c550->shadow_fcr |= PC16C550_FCR_FIFO_ENABLE;
		pc16c550->shadow_fcr |= PC16C550_FCR_RCVR_TRIGGER_4BYTE;
		//pc16c550->shadow_fcr |= PC16C550_FCR_RCVR_TRIGGER_8BYTE;
		// pc16c550->shadow_fcr |= PC16C550_FCR_RCVR_TRIGGER_14BYTE;
		rtuart_write_u8(&pc16c550->uart, PC16550_REG_FCR, pc16c550->shadow_fcr | PC16C550_FCR_RCVR_FIFO_RESET |PC16C550_FCR_XMIT_FIFO_RESET);
		rtuart_write_u8(&pc16c550->uart, PC16550_REG_FCR, pc16c550->shadow_fcr);
		// _pc16550_enable_fifo(pc16c550);
	}
	return &pc16c550->uart;
}
