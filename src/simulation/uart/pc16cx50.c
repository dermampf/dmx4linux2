
#include "pc16cx50_registers.h"
#include "pc16cx50.h"

#include <string.h>
#include <linux/types.h>

void pc16x50_init(struct pc16x50 * uart,
                  void (*reg_write) (void *, int, u8),
                  u8   (*reg_read)  (void *, int),
                  void *  reg_context
                  )
{
  memset(uart, 0, sizeof(*uart));
  uart->write_reg = reg_write;
  uart->read_reg  = reg_read;
  uart->reg_context = reg_context;
}


//=====[ generic register access functions ]===========

static void pc16x50_reg_write(struct pc16x50 * uart, int reg, u8 value)
{
  uart->write_reg(uart->reg_context, reg, value);
}

static u8 pc16x50_reg_read(struct pc16x50 * uart, int reg)
{
  return uart->read_reg(uart->reg_context, reg);
}

static void pc16x50_reg_set_bits(struct pc16x50 *uart, int reg, u8 bits)
{
  pc16x50_reg_write(uart, reg, pc16x50_reg_read(uart, reg) | bits);
}

static void pc16x50_reg_clear_bits(struct pc16x50 *uart, int reg, u8 bits)
{
  pc16x50_reg_write(uart, reg, pc16x50_reg_read(uart, reg) & ~bits);
}

static void pc16x50_reg_change_bits(struct pc16x50 *uart, int reg, u8 bits, u8 mask)
{
  pc16x50_reg_write(uart, reg, (~mask & pc16x50_reg_read(uart, reg)) | (mask & bits));
}


//=====[ specific register access functions ]=========


void pc16x50_write_thr(struct pc16x50 * uart, u8 value)
{
  pc16x50_reg_write(uart, PC16550_REG_THR, value);
}

u8 pc16x50_read_rbr(struct pc16x50 * uart)
{
  return pc16x50_reg_read(uart, PC16550_REG_RBR);
}


//===============================================

u8 pc16x50_read_ier(struct pc16x50 *uart)
{
  return pc16x50_reg_read(uart, PC16550_REG_IER);
}

void pc16x50_write_ier(struct pc16x50 *uart, u8 ier)
{
  pc16x50_reg_write(uart, PC16550_REG_IER, ier);
}

void pc16x50_set_ier_bits(struct pc16x50 *uart, u8 bits)
{
  pc16x50_reg_set_bits(uart, PC16550_REG_IER, bits);
}


void pc16x50_clear_ier_bits(struct pc16x50 *uart, u8 bits)
{
  pc16x50_reg_clear_bits(uart, PC16550_REG_IER, bits);
}


void pc16x50_change_ier_bits(struct pc16x50 *uart, u8 bits, u8 mask)
{
  pc16x50_reg_change_bits(uart, PC16550_REG_IER, bits, mask);
}

//================================================

u8 pc16x50_read_isr(struct pc16x50 *uart)
{
  return pc16x50_reg_read(uart, PC16550_REG_IIR);
}

//================================================


void pc16x50_flush_tx_fifo(struct pc16x50 * uart)
{
  pc16x50_reg_write(uart, PC16550_REG_FCR, uart->shadow_fcr | PC16C550_FCR_XMIT_FIFO_RESET);
}

void pc16x50_flush_rx_fifo(struct pc16x50 * uart)
{
  pc16x50_reg_write(uart, PC16550_REG_FCR, uart->shadow_fcr | PC16C550_FCR_RCVR_FIFO_RESET);
}

void pc16x50_flush_txrx_fifo(struct pc16x50 * uart)
{
  pc16x50_reg_write(uart, PC16550_REG_FCR, uart->shadow_fcr | PC16C550_FCR_RCVR_FIFO_RESET |PC16C550_FCR_XMIT_FIFO_RESET);
}

void pc16x50_enable_fifo(struct pc16x50 * uart)
{
  uart->shadow_fcr |= PC16C550_FCR_FIFO_ENABLE;
  pc16x50_reg_write(uart, PC16550_REG_FCR, uart->shadow_fcr);
}

void pc16x50_disable_fifo(struct pc16x50 * uart)
{
  uart->shadow_fcr &= ~PC16C550_FCR_FIFO_ENABLE;
  pc16x50_reg_write(uart, PC16550_REG_FCR, uart->shadow_fcr);
}

int pc16x50_set_fifo_trigger_level(struct pc16x50 * uart, int level)
{
  u8 fcr = uart->shadow_fcr;
  fcr &= ~PC16C550_FCR_RCVR_TRIGGER_MASK;
  switch(level)
    {
    case 1:
      fcr |= PC16C550_FCR_RCVR_TRIGGER_1BYTE;
      break;
    case 4:
      fcr |= PC16C550_FCR_RCVR_TRIGGER_4BYTE;
      break;
    case 8:
      fcr |= PC16C550_FCR_RCVR_TRIGGER_8BYTE;
      break;
    case 14:
      fcr |= PC16C550_FCR_RCVR_TRIGGER_14BYTE;
      break;
    default:
      return 1;
    }
  uart->shadow_fcr = fcr;
  pc16x50_reg_write(uart, PC16550_REG_FCR, fcr);
  return 0;
}



//====================================================

void pc16x50_set_baudrate_divisor  (struct pc16x50 * uart, u16 divisor)
{
  const u8 lcr = pc16x50_reg_read(uart, PC16550_REG_LCR);
  pc16x50_reg_write(uart, PC16550_REG_LCR, lcr | PC16550_LCR_DLAB);
  pc16x50_reg_write(uart, PC16550_REG_DLL, (u8)divisor);
  pc16x50_reg_write(uart, PC16550_REG_DLM, (u8)(divisor>>8));
  pc16x50_reg_write(uart, PC16550_REG_LCR, lcr);
}


u16 pc16x50_get_baudrate_divisor  (struct pc16x50 * uart)
{
  u16 divisor;
  const u8 lcr = pc16x50_reg_read(uart, PC16550_REG_LCR);
  pc16x50_reg_write(uart, PC16550_REG_LCR, lcr | PC16550_LCR_DLAB);
  divisor = pc16x50_reg_read (uart, PC16550_REG_DLL);
  divisor |= pc16x50_reg_read (uart, PC16550_REG_DLM) << 8;
  pc16x50_reg_write(uart, PC16550_REG_LCR, lcr);
  return divisor;
}


//====================================================

u8 pc16x50_read_lcr(struct pc16x50 *uart)
{
  return pc16x50_reg_read(uart, PC16550_REG_LCR);
}

void pc16x50_write_lcr(struct pc16x50 *uart, u8 lcr)
{
  pc16x50_reg_write(uart, PC16550_REG_LCR, lcr);
}

void pc16x50_set_lcr_bits(struct pc16x50 *uart, u8 lcr_bits)
{
  pc16x50_reg_set_bits(uart, PC16550_REG_LCR, lcr_bits);
}


void pc16x50_clear_lcr_bits(struct pc16x50 *uart, u8 lcr_bits)
{
  pc16x50_reg_clear_bits(uart, PC16550_REG_LCR, lcr_bits);
}


void pc16x50_change_lcr_bits(struct pc16x50 *uart, u8 lcr_bits, u8 lcr_mask)
{
  pc16x50_reg_change_bits(uart, PC16550_REG_LCR, lcr_bits, lcr_mask);
}



u8 pc16x50_update_format_bits (u8 lcr, const int databits, const char parity, const char stopbits)
{
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
  return lcr;
}


void pc16x50_set_format (struct pc16x50 * uart, const int databits, const char parity, const char stopbits)
{
  pc16x50_write_lcr(uart, pc16x50_update_format_bits (pc16x50_read_lcr(uart), databits, parity, stopbits));
}


void pc16x50_decode_format(const u8 lcr, int * databits, char * parity, int * stopbits)
{
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
}


void pc16x50_get_format(struct pc16x50 *uart, int * databits, char * parity, int * stopbits)
{
  pc16x50_decode_format(pc16x50_read_lcr(uart), databits, parity, stopbits);
}


void pc16x50_enable_break(struct pc16x50 *uart)
{
  pc16x50_set_lcr_bits(uart, PC16550_LCR_BREAK);
}


void pc16x50_disable_break(struct pc16x50 *uart)
{
  pc16x50_clear_lcr_bits(uart, PC16550_LCR_BREAK);
}


//==============================================================

u8 pc16x50_read_mcr(struct pc16x50 *uart)
{
  return uart->mcr_shadow;
}


void pc16x50_write_mcr(struct pc16x50 *uart, u8 mcr)
{
  uart->mcr_shadow = mcr;
  pc16x50_reg_write(uart, PC16550_REG_MCR, mcr);
}


void pc16x50_set_mcr_bits(struct pc16x50 *uart, u8 mcr_bits)
{
  pc16x50_write_mcr(uart, uart->mcr_shadow | mcr_bits);
}


void pc16x50_clear_mcr_bits(struct pc16x50 *uart, u8 mcr_bits)
{
  pc16x50_write_mcr(uart, uart->mcr_shadow & ~mcr_bits);
}


void pc16x50_change_mcr_bits(struct pc16x50 *uart, u8 mcr_bits, u8 mcr_mask)
{
  pc16x50_write_mcr(uart, (~mcr_mask & uart->mcr_shadow) | (mcr_mask & mcr_bits));
}

//================================================

u8 pc16x50_read_lsr(struct pc16x50 *uart)
{
  return pc16x50_reg_read(uart, PC16550_REG_LSR);
}


//================================================

u8 pc16x50_read_msr(struct pc16x50 *uart)
{
  return pc16x50_reg_read(uart, PC16550_REG_MSR);
}




