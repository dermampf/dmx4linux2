#include "rtuart.h"
#include "rtuart_priv.h"
#include "rtuart_values.h"
#include "rtuart_pl011.h"
#include <stdlib.h>
#include <stdio.h>

#include <kernel.h>

struct rtuart_pl011 {
	struct rtuart uart;
	u32           base_clock;
	struct rtuart_buffer * tx_buffer;
	struct rtuart_buffer * rx_buffer;
	int           in_irq; // make it an atomic. increment every time we enter an irq and decrement on exit.
	u32           old_input_state; /* datasheet states, that the delta bits are not available. */
	int           do_tx_polling;
	struct tasklet_struct irq_tasklet;
};

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

const char * pl011_regname(const int regno)
{
	static const char * _regnames[] =
	{
		"DR", // 0x00
		"RSRECR", // 0x04
		"0x08",  // 0x08
		"0x0C", // 0x0C
		"0x10", // 0x10
		"0x14", // 0x14
		"FR", // 0x18
		"0x1C", // 0x1C
		"0x20", // 0x20
		"IBRD", // 0x24
		"FBRD", // 0x28
		"LCRH", // 0x2C
		"CR", // 0x30
		"IFLS", // 0x34
		"IMSC", // 0x38
		"RIS", // 0x3C
		"MIS", // 0x40
		"ICR"  // 0x44
	};
	return (regno <= 0x44/4) ? _regnames[regno] : "???";
}

#define PL011_LCRH_SPS    (1<<7) /* stick parity */
#define	PL011_LCRH_WLEN_SHIFT 5 /* n=5..8 */
#define	PL011_LCRH_WLEN_MASK (3<<PL011_LCRH_WLEN_SHIFT)
#define	PL011_LCRH_FEN    (1<<4) /* fifo enable */
#define	PL011_LCRH_STP2   (1<<3) /* 1 = two stop bits, 0 = one stop bit */
#define	PL011_LCRH_EPS    (1<<2) /* 1 = even parity, 0 = odd partiy */
#define	PL011_LCRH_PEN    (1<<1) /* 1 = parity enabled, 0 = parity disabled */
#define	PL011_LCRH_BRK    (1<<0) /* 1 = break on, 0 = break off */

enum {
	PL011_DR_OE = (1<<11),
	PL011_DR_BE = (1<<10),
	PL011_DR_PE = (1<<9),
	PL011_DR_FE = (1<<8),
	PL011_DR_DATA = (0xff),
};

enum {
	PL011_RSRECR_OE = (1<<3),
	PL011_RSRECR_BE = (1<<2),
	PL011_RSRECR_PE = (1<<1),
	PL011_RSRECR_FE = (1<<0),
};

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
	PL011_IFLS_RXIFLSEL_MASK   = (3<<3),
	PL011_IFLS_RXIFLSEL_2BYTE  = (0<<3),
	PL011_IFLS_RXIFLSEL_4BYTE  = (1<<3),
	PL011_IFLS_RXIFLSEL_8BYTE  = (2<<3),
	PL011_IFLS_RXIFLSEL_12BYTE = (3<<3),
	PL011_IFLS_RXIFLSEL_14BYTE = (4<<3),

	PL011_IFLS_TXIFLSEL_MASK   = (3<<0),
	PL011_IFLS_TXIFLSEL_2BYTE  = (0<<0),
	PL011_IFLS_TXIFLSEL_4BYTE  = (1<<0),
	PL011_IFLS_TXIFLSEL_8BYTE  = (2<<0),
	PL011_IFLS_TXIFLSEL_12BYTE = (3<<0),
	PL011_IFLS_TXIFLSEL_14BYTE = (4<<0),
};

enum {
	PL011_MIS_OE = (1<<10),
	PL011_MIS_BE = (1<<9),
	PL011_MIS_PE = (1<<8),
	PL011_MIS_FE = (1<<7),
	PL011_MIS_RT = (1<<6),
	PL011_MIS_TX = (1<<5),
	PL011_MIS_RX = (1<<4),
	PL011_MIS_DSR = (1<<3),
	PL011_MIS_DCD = (1<<2),
	PL011_MIS_CTS = (1<<1),
	PL011_MIS_RI  = (1<<0)
};

enum {
	PL011_IS_OE  = PL011_MIS_OE,
	PL011_IS_BE  = PL011_MIS_BE,
	PL011_IS_PE  = PL011_MIS_PE,
	PL011_IS_FE  = PL011_MIS_FE,
	PL011_IS_RT  = PL011_MIS_RT,
	PL011_IS_TX  = PL011_MIS_TX,
	PL011_IS_RX  = PL011_MIS_RX,
	PL011_IS_DSR = PL011_MIS_DSR,
	PL011_IS_DCD = PL011_MIS_DCD,
	PL011_IS_CTS = PL011_MIS_CTS,
	PL011_IS_RI  = PL011_MIS_RI
};

enum {
	PL011_IMSC_OE  = PL011_MIS_OE,
	PL011_IMSC_BE  = PL011_MIS_BE,
	PL011_IMSC_PE  = PL011_MIS_PE,
	PL011_IMSC_FE  = PL011_MIS_FE,
	PL011_IMSC_RT  = PL011_MIS_RT,
	PL011_IMSC_TX  = PL011_MIS_TX,
	PL011_IMSC_RX  = PL011_MIS_RX,
	PL011_IMSC_DSR = PL011_MIS_DSR,
	PL011_IMSC_DCD = PL011_MIS_DCD,
	PL011_IMSC_CTS = PL011_MIS_CTS,
	PL011_IMSC_RI  = PL011_MIS_RI
};


enum { // PL011_REG_FR
	PL011_FR_RI   = (1<<8),
	PL011_FR_TXFE = (1<<7),
	PL011_FR_RXFF = (1<<6),
	PL011_FR_TXFF = (1<<5),
	PL011_FR_RXFE = (1<<4),
	PL011_FR_BUSY = (1<<3),
	PL011_FR_DCD  = (1<<2),
	PL011_FR_DSR  = (1<<1),
	PL011_FR_CTS  = (1<<0),
};


static unsigned int pl011_fr_to_event(const u32 fr)
{
	return
		((fr & PL011_FR_CTS) ? RTUART_EVENT_CTS_CHANGE : 0) |
		((fr & PL011_FR_DSR) ? RTUART_EVENT_DSR_CHANGE : 0) |
		((fr & PL011_FR_RI)  ? RTUART_EVENT_RI_CHANGE  : 0) |
		((fr & PL011_FR_DCD) ? RTUART_EVENT_DCD_CHANGE : 0);
}

static unsigned int pl011_is_to_event(const u32 is)
{
	return
		((is & PL011_IS_CTS) ? RTUART_EVENT_CTS_CHANGE : 0) |
		((is & PL011_IS_DSR) ? RTUART_EVENT_DSR_CHANGE : 0) |
		((is & PL011_IS_RI)  ? RTUART_EVENT_RI_CHANGE  : 0) |
		((is & PL011_IS_DCD) ? RTUART_EVENT_DCD_CHANGE : 0) |
		((is & PL011_IS_OE) ? RTUART_EVENT_OVERRUN_ERROR : 0) |
		((is & PL011_IS_PE) ? RTUART_EVENT_PARITY_ERROR : 0) |
		((is & PL011_IS_FE) ? RTUART_EVENT_FRAME_ERROR : 0) |
		((is & PL011_IS_BE) ? RTUART_EVENT_BREAK : 0)
		;
}

static unsigned int pl011_rsrecr_to_event(const u8 rsrecr)
{
	// later use uint64_t and use   return (LsrToEvent >> ((lsr>>1)&15)) & 15;
	// or use array of 16 uint8_t and do return LsrToEvent[(lsr>>1)&15];

	return  ((rsrecr & PL011_RSRECR_OE) ? RTUART_EVENT_OVERRUN_ERROR : 0) |
		((rsrecr & PL011_RSRECR_PE) ? RTUART_EVENT_PARITY_ERROR : 0) |
		((rsrecr & PL011_RSRECR_FE) ? RTUART_EVENT_FRAME_ERROR : 0) |
		((rsrecr & PL011_RSRECR_BE) ? RTUART_EVENT_BREAK : 0)
		;
}

static unsigned int pl011_dr_to_event(const u8 rsrecr)
{
	// later use uint64_t and use   return (LsrToEvent >> ((lsr>>1)&15)) & 15;
	// or use array of 16 uint8_t and do return LsrToEvent[(lsr>>1)&15];

	return  ((rsrecr & PL011_DR_OE) ? RTUART_EVENT_OVERRUN_ERROR : 0) |
		((rsrecr & PL011_DR_PE) ? RTUART_EVENT_PARITY_ERROR : 0) |
		((rsrecr & PL011_DR_FE) ? RTUART_EVENT_FRAME_ERROR : 0) |
		((rsrecr & PL011_DR_BE) ? RTUART_EVENT_BREAK : 0)
		;
}


static int pl011_read_register(struct rtuart * uart,
			       const int regno,
			       u32 * value)
{
	const int ret = rtuart_read_u32(uart, regno, value);
	printk(KERN_INFO"pl011_read_register(%s) => 0x%08lX\n",
	       pl011_regname(regno),
	       *value);
	return ret;
}

static int pl011_write_register(struct rtuart * uart,
				const int regno,
				const u32 value)
{
	printk(KERN_INFO"pl011_write_register(%s, 0x%08lX)\n",
	       pl011_regname(regno),
	       value);
	return rtuart_write_u32(uart, regno, value);
}

static int pl011_change_register(struct rtuart * uart,
				 const int regno,
				 const u32 mask,
				 const u32 value)
{
	printk(KERN_INFO"pl011_change_register(%s, mask:0x%08lX, value:0x%08lX)\n",
	       pl011_regname(regno),
	       mask, value);
	if (mask) {
		u32 old_lcrh;
		if (pl011_read_register(uart, regno, &old_lcrh))
			return -1;
		const u32 lcrh = (old_lcrh & ~mask) | (value & mask);
		if (lcrh != old_lcrh)
			if (rtuart_write_u32(uart, regno, lcrh))
				return -1;
	}
	return 0;
}

static void pl011_handle_transmitter_empty(struct rtuart_pl011 * pl011);
static void pl011_tx_stop (struct rtuart * uart, struct rtuart_buffer * buffer, int * allready_written);
static int  pl011_tx_written (struct rtuart * uart, struct rtuart_buffer * buffer);
static long pl011_rx_stop (struct rtuart * uart, struct rtuart_buffer * buffer);
static void pl011_handle_irq (struct rtuart * uart);




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

static int pl011_txfifo_is_full(struct rtuart *uart)
{
	u32 fr;
	return pl011_read_register(uart, PL011_REG_FR, &fr) || PL011_FR_ISTXFIFOFULL(fr);
}

static int pl011_txfifo_is_empty(struct rtuart *uart)
{
	u32 fr;
	return pl011_read_register(uart, PL011_REG_FR, &fr) || PL011_FR_ISTXFIFOEMPTY(fr);
}

static int pl011_rxfifo_is_full(struct rtuart *uart)
{
	u32 fr;
	return pl011_read_register(uart, PL011_REG_FR, &fr) || PL011_FR_ISRXFIFOFULL(fr);
}

static int pl011_rxfifo_is_empty(struct rtuart *uart)
{
	u32 fr;
	return pl011_read_register(uart, PL011_REG_FR, &fr) || PL011_FR_ISRXFIFOEMPTY(fr);
}

static int pl011_tx_is_not_empty(struct rtuart *uart)
{
	u32 fr;
	if (pl011_read_register(uart, PL011_REG_FR, &fr)) {
		printf("failed to read PL011_REG_FR\n");
		exit(1);
	}
	return PL011_FR_ISTXBUSY(fr);
}

static void spl011_set_rx_fifo_level(struct rtuart * uart, const int level)
{
	const u32 value =
		(level <= 2) ? PL011_IFLS_RXIFLSEL_2BYTE :
		(level <= 4) ? PL011_IFLS_RXIFLSEL_4BYTE :
		(level <= 8) ? PL011_IFLS_RXIFLSEL_8BYTE :
		(level <= 12) ? PL011_IFLS_RXIFLSEL_12BYTE :
		PL011_IFLS_RXIFLSEL_14BYTE;

	pl011_change_register(uart,
			      PL011_REG_IFLS,
			      PL011_IFLS_RXIFLSEL_MASK,
			      value
		);
}

static int spl011_rx_fifo_level(struct rtuart * uart)
{
	u32 value;
	if (pl011_read_register(uart,
			   PL011_REG_IFLS,
			   &value
		    ))
		return -1;
	switch (value & PL011_IFLS_RXIFLSEL_MASK)
	{
	case PL011_IFLS_RXIFLSEL_2BYTE: return 2;
	case PL011_IFLS_RXIFLSEL_4BYTE: return 4;
	case PL011_IFLS_RXIFLSEL_8BYTE: return 8;
	case PL011_IFLS_RXIFLSEL_12BYTE: return 12;
	case PL011_IFLS_RXIFLSEL_14BYTE: return 14;
	default:
		break;
	}
	return -1;
}

static void spl011_set_tx_fifo_level(struct rtuart * uart, const int level)
{
	const u32 value =
		(level < 4) ? PL011_IFLS_TXIFLSEL_2BYTE :
		(level < 8) ? PL011_IFLS_TXIFLSEL_4BYTE :
		(level < 12) ? PL011_IFLS_TXIFLSEL_8BYTE :
		(level < 14) ? PL011_IFLS_TXIFLSEL_12BYTE :
		PL011_IFLS_TXIFLSEL_14BYTE;

	pl011_change_register(uart,
			      PL011_REG_IFLS,
			      PL011_IFLS_TXIFLSEL_MASK,
			      value
		);
}

static int spl011_tx_fifo_level(struct rtuart * uart)
{
	u32 value;
	if (pl011_read_register(uart,
			   PL011_REG_IFLS,
			   &value
		    ))
		return -1;
	switch (value & PL011_IFLS_TXIFLSEL_MASK)
	{
	case PL011_IFLS_TXIFLSEL_2BYTE: return 2;
	case PL011_IFLS_TXIFLSEL_4BYTE: return 4;
	case PL011_IFLS_TXIFLSEL_8BYTE: return 8;
	case PL011_IFLS_TXIFLSEL_12BYTE: return 12;
	case PL011_IFLS_TXIFLSEL_14BYTE: return 14;
	default:
		break;
	}
	return -1;
}

static int pl011_set_fifo_enable(struct rtuart * uart, const int on)
{
	if (pl011_change_register(uart,
				  PL011_REG_LCRH,
				  PL011_LCRH_FEN,
				  on ? PL011_LCRH_FEN : 0))
		return -1;

	// we need to flush the fifos.

	spl011_set_rx_fifo_level(uart, 4 /*8*/);
	spl011_set_tx_fifo_level(uart, 8);
}

static int pl011_mod_control (struct rtuart * uart,
			       const u32 new_lcrh_mask,
			       const u32 new_lcrh_value,
			       const u32 new_cr_mask,
			       const u32 new_cr_value)
{
	if (pl011_change_register(uart,
				  PL011_REG_LCRH,
				  new_lcrh_mask,
				  new_lcrh_value))
		return -1;
	return pl011_change_register(uart,
				     PL011_REG_CR,
				     new_cr_mask,
				     new_cr_value);
}

static void pl011_enable_uart(struct rtuart * uart, const int on)
{
	pl011_mod_control (uart, 0, 0,
			   PL011_CR_RXE | PL011_CR_TXE | PL011_CR_UARTEN,
			   on ? (PL011_CR_RXE | PL011_CR_TXE | PL011_CR_UARTEN) : 0);
}


static int buffer_is_complete(struct rtuart_buffer * buffer)
{
	return !buffer || (buffer->transfered >= buffer->size) || (buffer->transfered >= buffer->validcount);
}

static void pl011_destroy   (struct rtuart * uart)
{
	if (uart)
		free(uart);
}

static void pl011_set_baudrate  (struct rtuart * uart, const unsigned long baudrate)
{
	struct rtuart_pl011 * pl011 = container_of(uart, struct rtuart_pl011, uart);
	const u32 divisor = (pl011->base_clock * 64) / (16 * baudrate);
	pl011_write_register(uart, PL011_REG_IBRD, (u16)(divisor >> 6));
	pl011_write_register(uart, PL011_REG_FBRD, divisor & 0x63);

	/* After changing the baudrate we need a dummy write on the lcrh register */
	u32 lcrh;
	if (pl011_read_register(uart, PL011_REG_LCRH, &lcrh) == 0)
		pl011_write_register(uart, PL011_REG_LCRH, lcrh);
}

static void pl011_get_baudrate  (struct rtuart * uart, unsigned long * baudrate)
{
	struct rtuart_pl011 * pl011 = container_of(uart, struct rtuart_pl011, uart);
	u32 ibrd, fbrd;
	pl011_read_register(uart, PL011_REG_IBRD, &ibrd);
	pl011_read_register(uart, PL011_REG_FBRD, &fbrd);
	const u32 divisor = ((ibrd&0xffff)<<6) | (fbrd & 0x3f);
	*baudrate  = (pl011->base_clock * 64) / (16 * divisor);
}

static void pl011_set_format    (struct rtuart * uart, const int databits, const char parity, const char stopbits)
{
	printk("pl011_set_format(databits:%d, parity:%c, stopbits:%d)\n", databits, parity, stopbits);
	const u32 mask = PL011_LCRH_WLEN_MASK |
		PL011_LCRH_PEN|PL011_LCRH_EPS |
		PL011_LCRH_STP2
		;

	const u32 value =
		(((databits-5)<<PL011_LCRH_WLEN_SHIFT) & PL011_LCRH_WLEN_MASK) |
		((parity=='e') ? (PL011_LCRH_PEN|PL011_LCRH_EPS) : (parity=='o') ? PL011_LCRH_PEN : 0) |
		((stopbits > 1) ? PL011_LCRH_STP2 : 0)
		;

	pl011_change_register(uart, PL011_REG_LCRH, mask, value);
}

static int pl011_get_format(struct rtuart *uart, int * databits, char * parity, int * stopbits)
{
	u32 lcr;
	if (pl011_read_register(uart, PL011_REG_LCRH, &lcr))
		return -1;
	*databits = ((lcr>>5)&3) + 5;
	*parity = (lcr & PL011_LCRH_PEN) ? ((lcr & PL011_LCRH_EPS) ? 'e' : 'o' ) : 'n';
	*stopbits = (lcr & PL011_LCRH_STP2) ? 2 : 1;
	return 0;
}

static void pl011_set_handshake (struct rtuart * uart, rtuart_handshake_t hs)
{
	(void)uart;
	printk("pl011_set_handshake(%d) - !! NOT YET IMPLEMENTED\n", hs);

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

	const int rtsctson =
		(hs == RTUART_HANDSHAKE_RTSCTS) ||
		(hs == RTUART_HANDSHAKE_RTSCTS_DSRDTR);

	pl011_mod_control (uart, 0, 0,
			   PL011_CR_CTSEN|PL011_CR_RTSEN,
			   rtsctson ? (PL011_CR_CTSEN|PL011_CR_RTSEN) : 0);
}

static u32 control_to_pl011_lcrh(const unsigned long control_mask)
{
	return (control_mask & RTUART_CONTROL_BREAK) ? PL011_LCRH_BRK : 0;
}

static u32 control_to_pl011_cr(const unsigned long control_mask)
{
	u32 cr = 0;
	if (control_mask & RTUART_CONTROL_RTS)
		cr |= PL011_CR_RTS;
	if (control_mask & RTUART_CONTROL_DTR)
		cr |= PL011_CR_DTR;
	if (control_mask & RTUART_CONTROL_OUT1)
		cr |= PL011_CR_OUT1;
	if (control_mask & RTUART_CONTROL_OUT2)
		cr |= PL011_CR_OUT2;
	if (control_mask & RTUART_CONTROL_LOOP)
		cr |= PL011_CR_LBE;
	return cr;
}

static void pl011_set_control (struct rtuart * uart, const unsigned long control_mask)
{
	const u32 lcrh = control_to_pl011_lcrh(control_mask);
	const u32 cr = control_to_pl011_cr(control_mask);
	pl011_mod_control (uart, lcrh, lcrh, cr, cr);
}

static void pl011_clr_control (struct rtuart * uart, const unsigned long control_mask)
{
	const u32 lcrh = control_to_pl011_lcrh(control_mask);
	const u32 cr = control_to_pl011_cr(control_mask);
	pl011_mod_control (uart, lcrh, 0, cr, 0);
}

static void pl011_get_control (struct rtuart * uart, unsigned long * control)
{
	u32 lcrh;
	u32 cr;
	*control = 0;
	if (!pl011_read_register(uart, PL011_REG_LCRH, &lcrh)) {
		if (lcrh & PL011_LCRH_BRK)
			*control |= RTUART_CONTROL_BREAK;
	}
	if (!pl011_read_register(uart, PL011_REG_CR, &cr)) {
		if (cr & PL011_CR_RTS)
			*control |= RTUART_CONTROL_RTS;

		if (cr & PL011_CR_DTR)
			*control |= RTUART_CONTROL_DTR;

		if (cr & PL011_CR_OUT1)
			*control |= RTUART_CONTROL_OUT1;

		if (cr & PL011_CR_OUT2)
			*control |= RTUART_CONTROL_OUT2;

		if (cr & PL011_CR_LBE)
			*control |= RTUART_CONTROL_LOOP;
	}
}

static void pl011_update_notification(struct rtuart * uart)
{
	int start_tx = 0;
	u32 cr;
	u32 imsc = 0;

	if ( (uart->notify_mask & RTUART_NOTIFY_RECEIVER_HASDATA) ||
	     (uart->notify_mask & RTUART_NOTIFY_RECEIVER_TIMEOUT) )
		imsc |= PL011_IMSC_RT|PL011_IMSC_RX;

	if (uart->notify_mask & RTUART_NOTIFY_TRANSMITTER_READY) {
		u32 current_imsc = 0;
		if (!pl011_read_register(uart, PL011_REG_IMSC, &current_imsc))
			start_tx = ((current_imsc & PL011_IMSC_TX) == 0);

		imsc |= PL011_IMSC_TX;
	}

	if (uart->notify_mask & RTUART_NOTIFY_TRANSMITTER_EMPTY) {
		imsc |= PL011_IMSC_TX;
	}


	 // Overrun, Break, Parity, Frame
	if (uart->notify_mask & RTUART_NOTIFY_RECEIVER_EVENT)
		imsc |= PL011_IMSC_OE|PL011_IMSC_BE|PL011_IMSC_PE|PL011_IMSC_FE;

	if (uart->notify_mask & RTUART_NOTIFY_RECEIVER_EVENT)
		imsc |= PL011_IMSC_CTS; // CTS, DSR, RI, DCD


	/* Enable or disable Transmitter and Receiver
	 * as they are needed.
	 */
	const u32 cr_value =
		(uart->notify_mask & (RTUART_NOTIFY_RECEIVER_HASDATA|
				      RTUART_NOTIFY_RECEIVER_TIMEOUT|
				      RTUART_NOTIFY_RECEIVER_EVENT))
		? PL011_CR_RXE
		: 0;
	pl011_change_register(uart,
			      PL011_REG_CR,
			      PL011_CR_RXE|PL011_CR_TXE,
			      cr_value|PL011_CR_TXE);

	pl011_write_register(uart, PL011_REG_IMSC, imsc);
}

static void pl011_set_notify (struct rtuart * uart, const unsigned long notify)
{
	struct rtuart_pl011 * pl011 = container_of(uart, struct rtuart_pl011, uart);
	unsigned long new_notify = uart->notify_mask | notify;
	if (uart->notify_mask != new_notify) {
		uart->notify_mask = new_notify;
		if (!pl011->in_irq)
			pl011_update_notification(uart);
	}
}

static void pl011_clr_notify (struct rtuart * uart, const unsigned long notify)
{
	struct rtuart_pl011 * pl011 = container_of(uart, struct rtuart_pl011, uart);
	unsigned long new_notify = uart->notify_mask & ~notify;
	if (uart->notify_mask != new_notify) {
		uart->notify_mask = new_notify;
		if (!pl011->in_irq)
			pl011_update_notification(uart);
	}
}

static void pl011_get_notify (struct rtuart * uart, unsigned long  * notify_mask)
{
	*notify_mask = uart->notify_mask;
}

static void pl011_tx_start (struct rtuart * uart, struct rtuart_buffer * buffer)
{
	struct rtuart_pl011 * pl011 = container_of(uart, struct rtuart_pl011, uart);
	buffer->transfered = 0;

	if (buffer->validcount > buffer->size) {
		printk(KERN_ERR"ERROR: tx_start: validcount(%d) > size(%d)\n",
		       buffer->validcount,
		       buffer->size
			);
		return;
	}

	if (pl011->tx_buffer) {
		int allready_written = 0;
		pl011_tx_stop (uart, buffer, &allready_written);
	}
	if (!pl011->tx_buffer) {
		pl011->tx_buffer = buffer;
		pl011->do_tx_polling = 0; // may move that to tx_stop
		pl011_set_notify (uart, RTUART_NOTIFY_TRANSMITTER_READY);
		tasklet_schedule(&pl011->irq_tasklet);
	}
}

static void pl011_tx_stop (struct rtuart * uart, struct rtuart_buffer * buffer, int * allready_written)
{
	struct rtuart_pl011 * pl011 = container_of(uart, struct rtuart_pl011, uart);

	if (pl011->tx_buffer == buffer) {
		// pl011->do_tx_polling = 0;
		if (allready_written)
			*allready_written = pl011_tx_written (uart, pl011->tx_buffer);
		pl011->tx_buffer = 0;
	}
}

static int  pl011_tx_written (struct rtuart * uart, struct rtuart_buffer * buffer)
{
	(void)uart;
	return buffer->transfered;
}

static void pl011_rx_start (struct rtuart * uart, struct rtuart_buffer * buffer)
{
	struct rtuart_pl011 * pl011 = container_of(uart, struct rtuart_pl011, uart);
	if (pl011->rx_buffer) {
		pl011_rx_stop (uart, pl011->rx_buffer);
	}

	buffer->transfered = 0;
	pl011->rx_buffer = buffer;
	pl011_set_notify (uart, RTUART_NOTIFY_RECEIVER_HASDATA|RTUART_NOTIFY_RECEIVER_TIMEOUT);
}

static long pl011_rx_stop (struct rtuart * uart, struct rtuart_buffer * buffer)
{
	struct rtuart_pl011 * pl011 = container_of(uart, struct rtuart_pl011, uart);
	pl011->rx_buffer = 0;
	pl011_clr_notify (uart, RTUART_NOTIFY_RECEIVER_HASDATA|RTUART_NOTIFY_RECEIVER_TIMEOUT);
	return 0;
}

static void pl011_rx_set_timeout(struct rtuart * uart, struct timespec * timeout)
{
	(void)uart;
	printk("pl011_rx_set_timeout\n");
}


static int copy_from_buffer_to_transmitter(struct rtuart_buffer * buffer,
					   struct rtuart_pl011 * pl011)
{
	int copied = 0;
	while (!pl011_txfifo_is_full(&pl011->uart)) {
		if ((buffer->transfered >= buffer->validcount) ||
		    (buffer->transfered >= buffer->size))
			break;
		const u8 c = buffer->data[buffer->transfered++];
		pl011_write_register(&pl011->uart, PL011_REG_DR, c);
		++copied;
	}
	return copied;
}



static void pl011_send_as_much_as_possible(struct rtuart_pl011 * pl011)
{
	struct rtuart * uart = &pl011->uart;
	int copied = 0;
	do
	{
		if (pl011->tx_buffer && (0 == pl011->tx_buffer->transfered))
			rtuart_call_tx_start (uart, pl011->tx_buffer);

		copied = copy_from_buffer_to_transmitter(pl011->tx_buffer,
							 pl011);

		if (buffer_is_complete(pl011->tx_buffer)) {
			uart->notify_mask &= ~RTUART_NOTIFY_TRANSMITTER_READY;

			struct rtuart_buffer * b = pl011->tx_buffer;
			pl011->tx_buffer = 0;
			rtuart_call_tx_end1(uart, b);

			if ((pl011->tx_buffer == 0) &&
			    (uart->notify_mask & RTUART_NOTIFY_TRANSMITTER_EMPTY)) {
				pl011->tx_buffer = b;
			}
		}
	} while ((copied > 0) && !pl011_txfifo_is_full(&pl011->uart) &&
		 (uart->notify_mask & RTUART_NOTIFY_TRANSMITTER_READY) &&
		 pl011->tx_buffer && (pl011->tx_buffer->transfered==0)
		);
}

static void pl011_fill_transmitter(struct rtuart_pl011 * pl011)
{
	const int remaining_tx_count = pl011->tx_buffer->validcount - pl011->tx_buffer->transfered;
	const int min_fifo_level = 2;
	const int fifo_level_offset = 2;
	if (remaining_tx_count < (min_fifo_level + fifo_level_offset))
		/* no fifo support for data less than 4 bytes.
		 * somehow do polling.
		 */
		pl011->do_tx_polling = 1;
	else {
		pl011->do_tx_polling = 0;
		spl011_set_tx_fifo_level(&pl011->uart, remaining_tx_count - fifo_level_offset);
	}

	pl011_send_as_much_as_possible(pl011);
}

static void pl011_handle_transmitter_empty(struct rtuart_pl011 * pl011)
{
	struct rtuart * uart = &pl011->uart;
	if (uart->notify_mask & RTUART_NOTIFY_TRANSMITTER_READY)
	{
		pl011_fill_transmitter(pl011);
	}
}

static void handle_received_data(struct rtuart_pl011 * pl011,
				 struct rtuart_buffer * buffer,
				 const int due_to_timeout)
{
	struct rtuart * uart = &pl011->uart;

	while(!pl011_rxfifo_is_empty(uart)) {
		u32 d;
		if (pl011_read_register(uart, PL011_REG_DR, &d))
			break;

		if (d & (PL011_DR_OE|PL011_DR_BE|PL011_DR_PE|PL011_DR_FE|PL011_DR_OE)) {
			const int event = pl011_dr_to_event(d);
			if (event)
				uart->cb->rx_err (uart, event);
		}
		else if (uart->notify_mask & RTUART_NOTIFY_RECEIVER_HASDATA) {
			if (!buffer ||
			    (buffer->transfered >= buffer->validcount) ||
			    (buffer->transfered >= buffer->size)) {
				uart->notify_mask &= ~RTUART_NOTIFY_RECEIVER_HASDATA;
				rtuart_call_rx_char (uart, (u8)d);
			}
			else {
				buffer->data[buffer->transfered++] = (u8)d;

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
			}

			//-- handle other callbacks.
			if (buffer) {
				if ((buffer->transfered >= buffer->validcount) ||
				    (buffer->transfered >= buffer->size)) {
					pl011->rx_buffer = 0;
					uart->notify_mask &= ~RTUART_NOTIFY_RECEIVER_HASDATA;
					rtuart_call_rx_end (uart, buffer);
				} else if (due_to_timeout)
					rtuart_call_rx_timeout (uart, buffer);
			}
			else
				printk(KERN_ERR"buffer is NULL in handle_received_data");
		}
	}
}

static void pl011_interrupt_handler_function (struct rtuart * uart)
{
	enum
	{
		ERRORS_INT_MASK = PL011_MIS_OE | PL011_MIS_BE | PL011_MIS_PE | PL011_MIS_FE
	};

	struct rtuart_pl011 * pl011 = container_of(uart, struct rtuart_pl011, uart);
	pl011->in_irq = 1;
	while (1)
	{
		u32 mis;
		pl011_read_register(uart, PL011_REG_MIS, &mis);

		const int have_remaining_tx_data = pl011->tx_buffer &&
			(pl011->tx_buffer->transfered < pl011->tx_buffer->validcount);

		if (((mis & PL011_MIS_TX)==0) &&   //-- have no tx-irq
		    have_remaining_tx_data &&      //-- have data available
		    pl011_txfifo_is_empty(uart)) { //-- txfifo is empty

		                //printf ("have_remaining_tx_data: transfered=%d validcount=%d\n", pl011->tx_buffer->transfered, pl011->tx_buffer->validcount);
			pl011_fill_transmitter(pl011);
		}

		else if (0==(mis & (PL011_MIS_OE|PL011_MIS_BE|PL011_MIS_PE|PL011_MIS_FE|PL011_MIS_RT|PL011_MIS_TX|PL011_MIS_RX|PL011_MIS_CTS))) {

			if ((uart->notify_mask & RTUART_NOTIFY_TRANSMITTER_EMPTY) &&
			    buffer_is_complete(pl011->tx_buffer)) {

				/*
				 * if NOTIFY_TRANSMITTER_EMPTY is set we send out the rest but we
				 * are not thru. We might not get an interrupt anymore so it can be
				 * nessesary to schedule the tasklet that calls the irq-handler.
				 */
				if (0 == pl011_txfifo_is_empty(uart))
					pl011->do_tx_polling = 1;

				/*
				 * The fifo is empty so we are only transmitting the shift register.
				 * Depending on the baudrate this can not take too long. We can just wai
				 * for it from the interrupt. (Or we might schedule our tasklet.)
				 */
				else if(pl011_tx_is_not_empty(uart))
					// loop for some more time, allmoast empty.
					continue; // instead of continue can do: pl011->do_tx_polling = 1;

				/*
				 * We are done. tx-fifo is empty and the transmitter is empty.
				 */
				else {
					uart->notify_mask &= ~RTUART_NOTIFY_TRANSMITTER_EMPTY;
					struct rtuart_buffer * b = pl011->tx_buffer;
					pl011->tx_buffer = 0;
					/* TODO: buffer gets lost if no tx_end2 call back exists - memory leak
					 * but what to do wih the buffer. We can not free it if it was
					 * allocated on the stack. The caller must take care of it.
					 */
					rtuart_call_tx_end2(uart, b);
				}
			}
			pl011_update_notification(uart);
			pl011->in_irq = 0;
			if (pl011->do_tx_polling)
				tasklet_schedule(&pl011->irq_tasklet);
			return;
		}

		else if (mis & (PL011_MIS_RX|PL011_MIS_RT)) {
			pl011_write_register(uart, PL011_REG_ICR, mis & (PL011_MIS_RX|PL011_MIS_RT));
			handle_received_data(pl011,
					     pl011->rx_buffer,
					     mis & PL011_MIS_RT);
		}

		else if (mis & ERRORS_INT_MASK ) {
			pl011_write_register(uart,
					     PL011_REG_ICR,
					     mis & ERRORS_INT_MASK);
			rtuart_call_rx_err (uart,
					    pl011_is_to_event(mis & ERRORS_INT_MASK));
		}

		else if (mis & PL011_MIS_TX) {
			pl011_write_register(uart, PL011_REG_ICR, PL011_MIS_TX);
			pl011_handle_transmitter_empty(pl011);
		}

		else if (mis & PL011_MIS_CTS) {
			u32 fr;
			pl011_write_register(uart, PL011_REG_ICR, PL011_MIS_CTS);
			if (pl011_read_register(uart, PL011_REG_FR, &fr)) {
				rtuart_call_input_change (&pl011->uart,
							  RTUART_EVENT_CTS_CHANGE,
							  (fr & PL011_FR_CTS) ? RTUART_EVENT_CTS_CHANGE : 0);
			}
		}
	}
}

static void pl011_irq_tasklet(unsigned long arg)
{
	pl011_interrupt_handler_function ((struct rtuart *)arg);
}

static void pl011_handle_irq (struct rtuart * uart)
{
	struct rtuart_pl011 * pl011 = container_of(uart, struct rtuart_pl011, uart);
	tasklet_schedule(&pl011->irq_tasklet);
}

static struct rtuart_ops rtuart_pl011_ops =
{
	.destroy       = pl011_destroy,
	.set_baudrate  = pl011_set_baudrate,
	.get_baudrate  = pl011_get_baudrate,
	.set_format    = pl011_set_format,
	.set_handshake = pl011_set_handshake,
	.set_control   = pl011_set_control,
	.clr_control   = pl011_clr_control,
	.get_control   = pl011_get_control,
	.set_notify    = pl011_set_notify,
	.clr_notify    = pl011_clr_notify,
	.get_notify    = pl011_get_notify,
	.tx_start      = pl011_tx_start,
	.tx_stop       = pl011_tx_stop,
	.tx_written    = pl011_tx_written,
	.rx_start      = pl011_rx_start,
	.rx_stop       = pl011_rx_stop,
	.rx_set_timeout = pl011_rx_set_timeout,
	.handle_irq    = pl011_handle_irq,
};

struct rtuart * rtuart_pl011_create(struct rtuart_bus * bus,
				    const unsigned long base_clock_hz)
{
	struct rtuart_pl011 * pl011 = malloc(sizeof(struct rtuart_pl011));
	if (pl011) {
		rtuart_init(&pl011->uart);
		pl011->uart.bus = bus;
		pl011->base_clock = base_clock_hz;
		pl011->uart.ops = &rtuart_pl011_ops;
		pl011->in_irq = 0;
		pl011->do_tx_polling = 0;

		tasklet_init(&pl011->irq_tasklet, pl011_irq_tasklet, (unsigned long)pl011);

		pl011_enable_uart(&pl011->uart, 1);
		pl011_set_fifo_enable(&pl011->uart, 1);
		pl011_update_notification(&pl011->uart);
	}
	return &pl011->uart;
}
