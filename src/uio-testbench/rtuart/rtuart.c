#include <rtuart.h>
#include <rtuart_priv.h>
#include <string.h>

int  rtuart_init(struct rtuart * uart)
{
	memset(uart, 0, sizeof(*uart));
	return 0;
}

void rtuart_destroy (struct rtuart * uart)
{
	if (uart->ops->destroy)
		uart->ops->destroy(uart);
}

void rtuart_assign_callbacks(struct rtuart * uart, struct rtuart_callbacks * cb, void * userptr)
{
	if (uart)
	{
		uart->cb = cb;
		uart->cb_user_ptr = userptr;
	}
}

void * rtuart_callback_user_ptr(struct rtuart * uart)
{
	return uart ? uart->cb_user_ptr : 0;
}

void rtuart_set_baudrate(struct rtuart * uart, const unsigned long baudrate)
{
	if (uart->ops->set_baudrate)
		uart->ops->set_baudrate(uart, baudrate);
}

void rtuart_get_baudrate(struct rtuart * uart, unsigned long * baudrate)
{
	if (uart->ops->get_baudrate)
		uart->ops->get_baudrate(uart, baudrate);
}

void rtuart_set_format(struct rtuart * uart, const int databits, const char parity, const char stopbits)
{
	if (uart->ops->set_format)
		uart->ops->set_format(uart, databits, parity, stopbits);
}

void rtuart_set_handshake(struct rtuart * uart, rtuart_handshake_t hs)
{
	if (uart->ops->set_handshake)
		uart->ops->set_handshake(uart, hs);
}

void rtuart_set_control(struct rtuart * uart, const unsigned long to_set)
{
	if (uart->ops->set_control)
		uart->ops->set_control(uart, to_set);
}

void rtuart_clr_control(struct rtuart * uart, const unsigned long to_clr)
{
	if (uart->ops->clr_control)
		uart->ops->clr_control(uart, to_clr);
}

void rtuart_get_control(struct rtuart * uart, unsigned long  * ctrl)
{
	if (uart->ops->get_control)
		uart->ops->get_control(uart, ctrl);
}

void rtuart_set_notify(struct rtuart * uart, const unsigned long notify_mask)
{
	if (uart->ops->get_notify)
		uart->ops->set_notify(uart, notify_mask);
}

void rtuart_clr_notify(struct rtuart * uart, const unsigned long notify_mask)
{
	if (uart->ops->clr_notify)
		uart->ops->clr_notify(uart, notify_mask);
}

void rtuart_get_notify(struct rtuart * uart, unsigned long  * notify_mask)
{
	if (uart->ops->get_notify)
		uart->ops->get_notify(uart, notify_mask);
}

void rtuart_tx_start (struct rtuart * uart, struct rtuart_buffer * buffer)
{
	if (uart->ops->tx_start)
		uart->ops->tx_start (uart, buffer);
}

void rtuart_tx_stop (struct rtuart * uart, struct rtuart_buffer * buffer, int * allready_written)
{
	if (uart->ops->tx_stop)
		uart->ops->tx_stop (uart, buffer, allready_written);
}

int  rtuart_tx_written(struct rtuart * uart, struct rtuart_buffer * buffer)
{
	return uart->ops->tx_written ? uart->ops->tx_written (uart, buffer) : -1;
}


void rtuart_rx_start (struct rtuart * uart, struct rtuart_buffer * buffer)
{
	if (uart->ops->rx_start)
		uart->ops->rx_start (uart, buffer);
}

long rtuart_rx_stop(struct rtuart * uart, struct rtuart_buffer * buffer)
{
	if (uart->ops->rx_stop)
		return uart->ops->rx_stop (uart, buffer);
	return -1;
}

void rtuart_rx_set_timeout(struct rtuart * uart, struct timespec * timeout)
{
	if (uart->ops->rx_set_timeout)
		uart->ops->rx_set_timeout (uart, timeout);
}


int rtuart_buffer_set_trigger(struct rtuart_buffer * buffer, int triggerno, int position)
{
	if (!buffer)
		return -1;
	if (triggerno >= RTUART_MAX_TRIGGERS)
		return -1;
	
	buffer->triggermask |= 1<<triggerno;
	buffer->trigger[triggerno] = position;
	return 0;
}



void rtuart_buffer_put(struct rtuart * uart,
		       struct rtuart_buffer * b,
		       const unsigned char c)
{
	if (b->data && (b->validcount < b->size)) {
		b->data[b->validcount] = c;
		++b->validcount;
		rtuart_handle_buffer_fill(uart, b);
	}
}

int rtuart_buffer_put_vector(struct rtuart * uart,
			     struct rtuart_buffer * b,
			     const unsigned char * c,
			     const int  size)
{
	if (b->data) {
		int i = 0;
		while ((i < size) && (b->validcount < b->size) ) {
		  b->data[b->validcount] = c[i];
			++b->validcount;
			++i;
		}
		rtuart_handle_buffer_fill(uart, b);
		return i;
	}
	return -1;
}

int rtuart_rx_buffer_valid(struct rtuart * uart, struct rtuart_buffer * b)
{
	return 1;
}

void rtuart_handle_buffer_fill(struct rtuart * uart, struct rtuart_buffer * b)
{
	rtuart_handle_trigger(uart, b);
	// buffer can be stopped and removed by a trigger.
	if (rtuart_rx_buffer_valid(uart, b) &&
	    (b->validcount >= b->size)) {
		if (uart->cb->rx_end)
			uart->cb->rx_end(uart, b);
	}
}

void rtuart_handle_trigger(struct rtuart * uart, struct rtuart_buffer * b)
{
	if (b->triggermask) {
		unsigned long triggermask = 0;
		int i;
		for (i = 0; i < RTUART_MAX_TRIGGERS; ++i) {
			if ((1<<i) & b->triggermask)
				if (b->trigger[i] >= b->validcount)
					triggermask |= 1<<i;
		}
		if (triggermask) {
			b->triggermask &= ~triggermask;
			if (uart->cb->rx_trigger)
				uart->cb->rx_trigger(uart, b, triggermask);
		}
	}
}

/*
  with rtuart_set_trigger(uart, buffer, N, buffer->size)
  rx_trigger would be called right before rx_end and
  with rtuart_set_trigger(uart, buffer, N, buffer->size+1)
  rx_trigger would never be called.
*/
