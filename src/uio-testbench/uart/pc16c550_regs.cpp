// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#include "pc16c550_regs.h"


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

#define pc16c550_reg_get_rbr(x)   (x)->read_register(PC16550_REG_RBR)
#define pc16c550_reg_set_thr(x,v) (x)->write_register(PC16550_REG_THR,v)
#define pc16c550_reg_get_ier(x)   (x)->read_register(PC16550_REG_IER)
#define pc16c550_reg_set_ier(x,v) (x)->write_register(PC16550_REG_IER,v)
#define pc16c550_reg_get_iir(x)   (x)->read_register(PC16550_REG_IIR)
#define pc16c550_reg_set_fcr(x,v) (x)->write_register(PC16550_REG_FCR,v)
#define pc16c550_reg_get_lcr(x)   (x)->read_register(PC16550_REG_LCR)
#define pc16c550_reg_set_lcr(x,v) (x)->write_register(PC16550_REG_LCR,v)
#define pc16c550_reg_get_mcr(x)   (x)->read_register(PC16550_REG_MCR)
#define pc16c550_reg_set_mcr(x,v) (x)->write_register(PC16550_REG_MCR,v)
#define pc16c550_reg_get_lsr(x)   (x)->read_register(PC16550_REG_LSR)
#define pc16c550_reg_set_lsr(x,v) (x)->write_register(PC16550_REG_LSR,v)
#define pc16c550_reg_get_msr(x)   (x)->read_register(PC16550_REG_MSR)
#define pc16c550_reg_set_msr(x,v) (x)->write_register(PC16550_REG_MSR,v)
#define pc16c550_reg_get_scr(x)   (x)->read_register(PC16550_REG_SCR)
#define pc16c550_reg_set_scr(x,v) (x)->write_register(PC16550_REG_SCR,v)
#define pc16c550_reg_get_dll(x)   (x)->read_register(PC16550_REG_DLL)
#define pc16c550_reg_set_dll(x,v) (x)->write_register(PC16550_REG_DLL,v)
#define pc16c550_reg_get_dlm(x)   (x)->read_register(PC16550_REG_DLM)
#define pc16c550_reg_set_dlm(x,v) (x)->write_register(PC16550_REG_LCR,v)
