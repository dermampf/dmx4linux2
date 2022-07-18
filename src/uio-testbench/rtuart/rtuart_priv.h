#pragma once

struct rtuart_ops;
struct rtuart_bus;
struct rtuart
{
	struct rtuart_callbacks * cb;
	void * cb_user_ptr;
	struct rtuart_ops *ops;
        struct rtuart_bus * bus;
	unsigned long  notify_mask;
};
struct rtuart_buffer;


/*
 * Returns 0 if initialization was successfull.
 */
int  rtuart_init(struct rtuart *);


struct rtuart_ops
{
	void (*destroy)       (struct rtuart * uart);
	void (*set_baudrate)  (struct rtuart * uart, const unsigned long baudrate);
	void (*get_baudrate)  (struct rtuart * uart, unsigned long * baudrate);
	void (*set_format)    (struct rtuart * uart, const int databits, const char parity, const char stopbits);
	void (*set_handshake) (struct rtuart * uart, rtuart_handshake_t hs);

	void (*set_control)   (struct rtuart * uart, const unsigned long control_mask);
	void (*clr_control)   (struct rtuart * uart, const unsigned long control_mask);
	void (*get_control)   (struct rtuart * uart, unsigned long * control);

	void (*set_notify) (struct rtuart *, const unsigned long set_notify);
	void (*clr_notify) (struct rtuart *, const unsigned long clear_notify);
	void (*get_notify) (struct rtuart *, unsigned long  * notify_mask);

	void (*tx_start)      (struct rtuart * uart, struct rtuart_buffer * buffer);
	void (*tx_stop)       (struct rtuart * uart, struct rtuart_buffer * buffer, int * allready_written);
	int  (*tx_written)    (struct rtuart * uart, struct rtuart_buffer * buffer);
	void (*rx_start)      (struct rtuart * uart, struct rtuart_buffer * buffer);
	long (*rx_stop)       (struct rtuart * uart, struct rtuart_buffer * buffer);
	void (*rx_set_timeout)(struct rtuart * uart, struct timespec * timeout);
	void (*handle_irq)    (struct rtuart * uart);
};


void rtuart_buffer_put(struct rtuart * uart,
		       struct rtuart_buffer * b,
		       const unsigned char c);
int rtuart_buffer_put_vector(struct rtuart * uart,
			     struct rtuart_buffer * b,
			     const unsigned char * c,
			     const int  size);
void rtuart_handle_buffer_fill(struct rtuart * uart, struct rtuart_buffer * b);
void rtuart_handle_trigger(struct rtuart * uart, struct rtuart_buffer * b);

static inline void rtuart_handle_irq(struct rtuart * uart)
{
	if (uart && uart->ops && uart->ops->handle_irq)
		return uart->ops->handle_irq(uart);
}


#include <rtuart_bus.h>

static inline int rtuart_read_u8(struct rtuart * u, const int reg, u8 * value)
{
  return rtuart_bus_read_u8(u->bus, reg, value);
}

static inline int rtuart_read_u16(struct rtuart * u, const int reg, u16 * value)
{
  return rtuart_bus_read_u16(u->bus, reg, value);
}

static inline int rtuart_read_u32(struct rtuart * u, const int reg, u32 * value)
{
  return rtuart_bus_read_u32(u->bus, reg, value);
}

static inline int rtuart_write_u8(struct rtuart * u, const int reg, const u8 value)
{
  return rtuart_bus_write_u8(u->bus, reg, value);
}

static inline int rtuart_write_u16(struct rtuart * u, const int reg, const u16 value)
{
  return rtuart_bus_write_u16(u->bus, reg, value);
}

static inline int rtuart_write_u32(struct rtuart * u, const int reg, const u32 value)
{
  return rtuart_bus_write_u32(u->bus, reg, value);
}





static inline void rtuart_call_tx_start (struct rtuart * uart,
					 struct rtuart_buffer * buffer)
{
	if (uart->cb && uart->cb->tx_start)
		uart->cb->tx_start (uart, buffer);
}


static inline void rtuart_call_tx_end1 (struct rtuart * uart,
					struct rtuart_buffer * buffer)
{
	if (uart->cb && uart->cb->tx_end1)
		uart->cb->tx_end1 (uart, buffer);
}

static inline void rtuart_call_tx_end2 (struct rtuart * uart,
					struct rtuart_buffer * buffer)
{
	if (uart->cb && uart->cb->tx_end2)
		uart->cb->tx_end2(uart, buffer);
}

static inline void rtuart_call_input_change (struct rtuart * uart,
					     unsigned long eventmask,
					     unsigned long values)
{
	if (uart->cb && uart->cb->input_change)
		uart->cb->input_change(uart, eventmask, values);
}

static inline void rtuart_call_rx_err (struct rtuart * uart,
				       unsigned long errmask)
{
	if (uart->cb && uart->cb->rx_err)
		uart->cb->rx_err(uart, errmask);
}

static inline void rtuart_call_rx_char (struct rtuart * uart,
					const unsigned short c)
{
	if (uart->cb && uart->cb->rx_char)
		uart->cb->rx_char (uart, c);
}

static inline void rtuart_call_rx_end (struct rtuart * uart,
				       struct rtuart_buffer * buffer)
{
	if (uart->cb && uart->cb->rx_end)
		uart->cb->rx_end(uart, buffer);
}

static inline void rtuart_call_rx_timeout (struct rtuart * uart,
					   struct rtuart_buffer * buffer)
{
	if (uart->cb && uart->cb->rx_timeout)
		uart->cb->rx_timeout(uart, buffer);
}

static inline void rtuart_call_rx_trigger (struct rtuart * uart,
					   struct rtuart_buffer * buffer,
					   const unsigned long triggermask
	)
{
	if (uart->cb && uart->cb->rx_trigger)
		uart->cb->rx_trigger(uart,
				     buffer,
				     triggermask);
}
