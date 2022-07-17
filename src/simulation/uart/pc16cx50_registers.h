#ifndef PC16Cx50_REGISTERS_H
#define PC16Cx50_REGISTERS_H

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
	PC16C550_FCR_RCVR_TRIGGER_MASK   = (3<<6),
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

#endif