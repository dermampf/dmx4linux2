#ifndef PC16Cx50_H
#define PC16Cx50_H

#include <linux/types.h>

struct pc16x50
{
  void (*write_reg) (void *  reg_context, int reg, __u8 value);
  __u8 (*read_reg)  (void *  reg_context, int reg);
  void *  reg_context;
  __u8 shadow_fcr;
  __u8 mcr_shadow;
};

typedef __u8  u8;
typedef __u16 u16;

void pc16x50_init(struct pc16x50 * uart,
                  void (*reg_write) (void *, int, __u8),
                  __u8 (*reg_read)  (void *, int),
                  void *  reg_context
                  );


void pc16x50_write_thr(struct pc16x50 * uart, u8 value);
u8   pc16x50_read_rbr(struct pc16x50 * uart);

u8   pc16x50_read_ier(struct pc16x50 *uart);
void pc16x50_write_ier(struct pc16x50 *uart, u8 ier);
void pc16x50_set_ier_bits(struct pc16x50 *uart, u8 bits);
void pc16x50_clear_ier_bits(struct pc16x50 *uart, u8 bits);
void pc16x50_change_ier_bits(struct pc16x50 *uart, u8 bits, u8 mask);

u8   pc16x50_read_isr(struct pc16x50 *uart);

void pc16x50_flush_tx_fifo(struct pc16x50 * uart);
void pc16x50_flush_rx_fifo(struct pc16x50 * uart);
void pc16x50_flush_txrx_fifo(struct pc16x50 * uart);
void pc16x50_enable_fifo(struct pc16x50 * uart);
void pc16x50_disable_fifo(struct pc16x50 * uart);
int  pc16x50_set_fifo_trigger_level(struct pc16x50 * uart, int level);

void pc16x50_set_baudrate_divisor  (struct pc16x50 * uart, u16 divisor);
u16  pc16x50_get_baudrate_divisor  (struct pc16x50 * uart);
u8   pc16x50_read_lcr(struct pc16x50 *uart);
void pc16x50_write_lcr(struct pc16x50 *uart, u8 lcr);
void pc16x50_set_lcr_bits(struct pc16x50 *uart, u8 lcr_bits);
void pc16x50_clear_lcr_bits(struct pc16x50 *uart, u8 lcr_bits);
void pc16x50_change_lcr_bits(struct pc16x50 *uart, u8 lcr_bits, u8 lcr_mask);
u8   pc16x50_update_format_bits (u8 lcr, const int databits, const char parity, const char stopbits);
void pc16x50_set_format (struct pc16x50 * uart, const int databits, const char parity, const char stopbits);
void pc16x50_decode_format(const u8 lcr, int * databits, char * parity, int * stopbits);
void pc16x50_get_format(struct pc16x50 *uart, int * databits, char * parity, int * stopbits);

void pc16x50_enable_break(struct pc16x50 *uart);
void pc16x50_disable_break(struct pc16x50 *uart);

u8   pc16x50_read_mcr(struct pc16x50 *uart);
void pc16x50_write_mcr(struct pc16x50 *uart, u8 mcr);
void pc16x50_set_mcr_bits(struct pc16x50 *uart, u8 mcr_bits);
void pc16x50_clear_mcr_bits(struct pc16x50 *uart, u8 mcr_bits);
void pc16x50_change_mcr_bits(struct pc16x50 *uart, u8 mcr_bits, u8 mcr_mask);

u8  pc16x50_read_lsr(struct pc16x50 *uart);

u8  pc16x50_read_msr(struct pc16x50 *uart);

#endif
