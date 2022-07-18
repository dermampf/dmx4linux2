#include <rtuart.h>
#include <rtuart_priv.h>
#include <rtuart_values.h>
#include <rtuart_factory.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <kernel.h>

#include <linux/dmx512/dmx512frame.h>
#include "dmx512_port.h"
#include "rdm_priv.h"

#include <gpiod.h> // For gpiod_*.

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h> // for posix timer functions.
#include <time.h> // for posix timer functions.


/* We use the upper 254 bytes of the dmx-buffer to store
 * the data that is send or received with a discovery unique branch
 * over the serial port. This is possible, because an rdm message
 * fits into the lower 258 bytes (256 + 2 bytes crc).
 */
enum {
  RDM_POS_DISC_UNIQUE_BANCH_DATA = 256+2
};

/*----------- put this into: dmx512_slotcount_lock.h ------------*/

typedef union slot_count_lock {
	struct {
		u32  enabled : 1;     /* the slot count lock is enabled */
		u32  locked  : 1;      /* the slot count is locked in */
		u32  required_count : 6;  /* The number of frames that are required for the lock to snap in */
		u32  matching_frame_count : 6; /* The number of currently valid frames */
		u32  matching_slot_count : 10; /* The slot count that has matched for the last <matching_frame_count> frames */
	};
	u32  data;
} slot_count_lock_t;

static void slot_count_lock__init(slot_count_lock_t * l)
{
	l->data = 0;
	l->required_count = 10; /* 10 consecutive frames of the same size are required */
}

/*
 * TODO: we shall also have a timeout of 1 second after receiving the last
 * dmx-frame with startcode 0 and we shall call slot_count_lock__lost_lock after
 * that 1 second timeout.
 */
static void slot_count_lock__lost_lock(slot_count_lock_t * l)
{
	l->locked = 0;
	l->matching_frame_count = 0;
	l->matching_slot_count = 0;
	printk(KERN_INFO"lost DMX-00 lock\n");
}

static void slot_count_lock__received_frame(slot_count_lock_t * l,
				       const struct dmx512frame * frame)
{
	if (l->enabled && (frame->startcode==0)) {
		if (l->locked) {
			if (frame->payload_size != l->matching_slot_count)
				slot_count_lock__lost_lock(l);
		}
		else {
			if (l->matching_frame_count &&
			    (l->matching_slot_count == frame->payload_size)) {
				if (l->matching_frame_count == l->required_count) {
					l->locked = 1;
					printf("DMX-00 locked in to %d slots\n",
					       l->matching_slot_count);
				}
				else
					++l->matching_frame_count;
			}
			else {
				//-- either frame_count is 0 or sizes does not match.
				l->matching_frame_count = 1;
				l->matching_slot_count = frame->payload_size;
			}
		}
	}
}
/*----------- END-OF: dmx512_slotcount_lock.h ------------*/

/*----------- START-OF: RDM support functions ---------- */

static int dmx512_is_rdm_frame(const struct dmx512frame * frame)
{
	return  (frame->data[DMX_POS_STARTCODE] == SC_RDM) &&
		(frame->data[RDM_POS_SUBSTARTCODE] == SC_SUB_MESSAGE);
}

static int dmx512_is_rdm_disc_unique_branch_request(const struct dmx512frame * frame)
{
  
	return  dmx512_is_rdm_frame(frame) &&
		(frame->data[RDM_POS_COMMAND_CLASS] == DISCOVERY_COMMAND) &&
		(frame->data[RDM_POS_PID]   == (u8)(DISC_UNIQUE_BRANCH>>8)) &&
		(frame->data[RDM_POS_PID+1] == (u8)(DISC_UNIQUE_BRANCH)) &&
		(frame->data[RDM_POS_PDL] == 12);
}

static int dmx512_is_rdm_disc_unique_branch_reply(const struct dmx512frame * frame)
{
	return  dmx512_is_rdm_frame(frame) &&
		(frame->data[RDM_POS_COMMAND_CLASS] == DISCOVERY_COMMAND_RESPONSE) &&
		(frame->data[RDM_POS_PID]   == (u8)(DISC_UNIQUE_BRANCH>>8)) &&
		(frame->data[RDM_POS_PID+1] == (u8)(DISC_UNIQUE_BRANCH)) &&
		(frame->data[RDM_POS_PDL] == 0);
}

static int dmx512_is_rdm_request(const struct dmx512frame * frame)
{
	return  dmx512_is_rdm_frame(frame) &&
		((frame->data[RDM_POS_COMMAND_CLASS] == DISCOVERY_COMMAND) ||
		 (frame->data[RDM_POS_COMMAND_CLASS] == GET_COMMAND) ||
		 (frame->data[RDM_POS_COMMAND_CLASS] == SET_COMMAND) );
}

static int dmx512_is_rdm_reply(const struct dmx512frame * frame)
{
	return  dmx512_is_rdm_frame(frame) &&
		((frame->data[RDM_POS_COMMAND_CLASS] == DISCOVERY_COMMAND_RESPONSE) ||
		 (frame->data[RDM_POS_COMMAND_CLASS] == GET_COMMAND_RESPONSE) ||
		 (frame->data[RDM_POS_COMMAND_CLASS] == SET_COMMAND_RESPONSE) );
}

static unsigned long long dmx512_rdm_frame_dest(const struct dmx512frame * frame)
{
#if 1
	unsigned long long uid = 0;
	int i;
	for (i = 0; i < 6; ++i)
		uid = (uid<<8) | frame->data[RDM_POS_DEST_UID+i];
	return uid;
#else
	return (frame->data[RDM_POS_DEST_UID+0] << (8*5)) |
		(frame->data[RDM_POS_DEST_UID+1] << (8*4)) |
		(frame->data[RDM_POS_DEST_UID+2] << (8*3)) |
		(frame->data[RDM_POS_DEST_UID+3] << (8*2)) |
		(frame->data[RDM_POS_DEST_UID+4] << (8*1)) |
		(frame->data[RDM_POS_DEST_UID+5] << (8*0)) ;
#endif
}

static int dmx512_is_rdm_frame_is_broadcast(const struct dmx512frame * frame)
{
	// return dmx512_rdm_frame_dest(&frame->frame) == 0xFFFFFFFFFFFFLL;

	return  (frame->data[RDM_POS_DEST_UID+0] == 0xFF) &&
		(frame->data[RDM_POS_DEST_UID+1] == 0xFF) &&
		(frame->data[RDM_POS_DEST_UID+2] == 0xFF) &&
		(frame->data[RDM_POS_DEST_UID+3] == 0xFF) &&
		(frame->data[RDM_POS_DEST_UID+4] == 0xFF) &&
		(frame->data[RDM_POS_DEST_UID+5] == 0xFF);
}


static u16 dmx512frame_calculate_rdm_checksum(const u8 * data, const int size)
{
	u16 sum = 0;
	int i;
	for (i = 0; i < size; ++i)
		sum += data[i];
	return sum;
}

static int dmx512_rdm_build_rdmdiscovery_reply(
	u8 * data,
	const struct dmx512frame * frame)
{
	u8 * d = data;
	// if (dmx512frame_is_rdm_discover_reply(port->tx.ptr, port->tx.remain)) {
	u16 checksum = 0;
	*(d++) = 0xFE; // 1
	*(d++) = 0xFE; // 2
	*(d++) = 0xFE; // 3
	*(d++) = 0xFE; // 4
	*(d++) = 0xFE; // 5
	*(d++) = 0xFE; // 6
	*(d++) = 0xFE; // 7
	*(d++) = 0xAA; // 8
	*(d++) = 0xAA | frame->data[RDM_POS_SRC_UID]; // 9
	*(d++) = 0x55 | frame->data[RDM_POS_SRC_UID]; // 10
	*(d++) = 0xAA | frame->data[RDM_POS_SRC_UID+1]; // 11
	*(d++) = 0x55 | frame->data[RDM_POS_SRC_UID+1]; // 12
	*(d++) = 0xAA | frame->data[RDM_POS_SRC_UID+2]; // 13
	*(d++) = 0x55 | frame->data[RDM_POS_SRC_UID+2]; // 14
	*(d++) = 0xAA | frame->data[RDM_POS_SRC_UID+3]; // 15
	*(d++) = 0x55 | frame->data[RDM_POS_SRC_UID+3]; // 16
	*(d++) = 0xAA | frame->data[RDM_POS_SRC_UID+4]; // 17
	*(d++) = 0x55 | frame->data[RDM_POS_SRC_UID+4]; // 18
	*(d++) = 0xAA | frame->data[RDM_POS_SRC_UID+5]; // 19
	*(d++) = 0x55 | frame->data[RDM_POS_SRC_UID+5]; // 20
	checksum = dmx512frame_calculate_rdm_checksum(data+8, 12);
	*(d++) = 0xAA | (u8)(checksum>>8); // 21
	*(d++) = 0x55 | (u8)(checksum>>8); // 22
	*(d++) = 0xAA | (u8)checksum; // 23
	*(d++) = 0x55 | (u8)checksum; // 24
	return d - data;
}

/*----------- END-OF: RDM support functions ---------- */

enum {
	DISCOVERY_REQUEST_REPLY_TIMEOUT = 15000, /*us*/
	TIMEOUT_RDMDISCOVERY_LENGTH = 15000,  /* we expect all rdm discovery reply data to be received then */

	TIMEOUT_RDMFRAME_PEROCTET = 60,
	TIMEOUT_RDMFRAME_STATIC = 3000,
	TIMEOUT_RDM_EOP_TO_SOP = 15000, /* end of request to start of reply */

	/* start of packet to end of the same packet */
	TIMEOUT_RDM_SOP_TO_EOP = 13000 + TIMEOUT_RDMFRAME_PEROCTET*256,
};

/*-------- DMX State ------------------------------*/


enum dmx512_uart_state {
	PORT_STATE_DOWN = 0, // just constructed, not yet up.
	PORT_STATE_IDLE,
	// mostly the same as IDLE. It is only entered if frame was received with a size defined by the slotcount-counter (< 512 slots)
	PORT_STATE_IDLE_AFTER_DMXDATA00,

	PORT_STATE_EXPECT_STARTCODE,
	PORT_STATE_RECEIVE_DMXDATA,

	PORT_STATE_TRANSMIT_BREAK_ONEWAY,
	PORT_STATE_TRANSMIT_DATA_ONEWAY,

	PORT_STATE_TRANSMIT_BREAK_TWOWAY,
	PORT_STATE_TRANSMIT_DATA_TWOWAY,

	PORT_STATE_RECV_RDMDISC_REPLY,
	PORT_STATE_RECV_RDMDISC_REPLY_COLLISION,

	PORT_STATE_EXPECT_RDM_REPLY_BREAK,
	PORT_STATE_RECV_RDM_REPLY_DATA,

	PORT_STATE_COUNT
};


static const char * statename (const enum dmx512_uart_state state)
{
	switch(state)
	{
	case PORT_STATE_DOWN:                  return "DOWN";
	case PORT_STATE_IDLE:                  return "IDLE";
	case PORT_STATE_IDLE_AFTER_DMXDATA00:  return  "IDLE_AFTER_DMXDATA00";

	case PORT_STATE_EXPECT_STARTCODE:      return "EXPECT_STARTCODE";
	case PORT_STATE_RECEIVE_DMXDATA:       return "RECEIVE_DMXDATA";

	case PORT_STATE_TRANSMIT_BREAK_ONEWAY: return "TRANSMIT_BREAK_ONEWAY";
	case PORT_STATE_TRANSMIT_DATA_ONEWAY:  return "TRANSMIT_DATA_ONEWAY";

	case PORT_STATE_TRANSMIT_BREAK_TWOWAY: return "TRANSMIT_BREAK_TWOWAY";
	case PORT_STATE_TRANSMIT_DATA_TWOWAY:  return "TRANSMIT_DATA_TWOWAY";

	case PORT_STATE_RECV_RDMDISC_REPLY:    return "RECV_RDMDISC_REPLY";
	case PORT_STATE_RECV_RDMDISC_REPLY_COLLISION: return "RECV_RDMDISC_REPLY_COLLISION";

	case PORT_STATE_EXPECT_RDM_REPLY_BREAK: return "EXPECT_RDM_REPLY_BREAK";
	case PORT_STATE_RECV_RDM_REPLY_DATA:    return "RECV_RDM_REPLY_DATA";

	default:
		break;
	}
	return "???";
};


#if 1
#define next_state(p,s)  { printk(KERN_NOTICE"change state from %s to %s in %s\n", statename ((p)->state), statename (s), __func__); (p)->state = s; }
#define next_state_label(p,s,l)  { printk(KERN_NOTICE"change state from %s to %s in %s:%s\n", statename ((p)->state), statename (s), __func__, l); (p)->state = s; }
#else
#define next_state(p,s)          { (p)->state = s; }
#define next_state_label(p,s,l)  { (p)->state = s; }
#endif

/*--------------------------------------*/


struct dmx512_uart_port
{
	struct rtuart *    uart;
	struct dmx512_port dmx;

	struct {
		struct dmx512_framequeue_entry * frame;
		struct rtuart_buffer buffer;
	} tx;

	struct {
		struct dmx512_framequeue_entry * frame;
		struct rtuart_buffer buffer;
	} rx;

	slot_count_lock_t slot_count_lock;

	void (*enable_rs485_transmitter)(struct dmx512_uart_port * port,
					 const int on);

	struct {
		struct gpiod_chip * chip;
		struct gpiod_line * line;
	} gpio_dmxtxen;

	enum dmx512_uart_state state;

	struct {
		int     active;
		timer_t id;
		struct sigevent se;
		struct itimerspec ts;
		int callTimeoutHandler;
		void (*func)(struct dmx512_uart_port * port);
	} rdm_timer;
};

// these two functions are for typesafety and for ease of use.
static inline struct dmx512_uart_port * rtuart_to_dmx512_uart_port(struct rtuart *uart)
{
	return (struct dmx512_uart_port *)(uart ? uart->cb_user_ptr : 0);
}

static inline struct dmx512_uart_port * dmx512port_to_dmx512_uart_port(struct dmx512_port *dmx)
{
	return container_of(dmx, struct dmx512_uart_port, dmx);
}





/*------ RDM timeout functions -----*/


static void dmx512rtuart_stop_rdm_timer(struct dmx512_uart_port * port)
{
	//-- posix timer function
	if (port->rdm_timer.active) {
		port->rdm_timer.active = 0;
		timer_delete(port->rdm_timer.id);
	}
	printk(KERN_INFO"dmx512rtuart_stop_rdm_timer()\n");
}

static void __posix_timer_thread_handler(sigval_t arg)
{
	struct dmx512_uart_port * port = (struct dmx512_uart_port *)arg.sival_ptr;
	port->rdm_timer.callTimeoutHandler = 1;
	printk (KERN_INFO"#### posix timeout handler called\n");
}


static void dmx512rtuart_start_rdm_timer(struct dmx512_uart_port * port,
					 const unsigned long timeout_microseconds)
{
	printk(KERN_INFO"#### dmx512rtuart_start_rdm_timer@0(%lu us)\n",
	       timeout_microseconds);

	int status;
	if (!port->rdm_timer.active) {
		port->rdm_timer.se.sigev_notify = SIGEV_THREAD;
		port->rdm_timer.se.sigev_value.sival_ptr = port;
		port->rdm_timer.se.sigev_notify_function = __posix_timer_thread_handler;
		port->rdm_timer.se.sigev_notify_attributes = NULL;
		status = timer_create(CLOCK_REALTIME,
					  &port->rdm_timer.se,
					  &port->rdm_timer.id);
		if (status == -1)
		{
			printk (KERN_ERR"failed to create timer\n");
			return;
		}
		port->rdm_timer.active = 1;
	}
	printk(KERN_INFO"dmx512rtuart_start_rdm_timer@1(%lu us)\n",
	       timeout_microseconds);

	const long long nanosecs = timeout_microseconds * 1000;
	port->rdm_timer.ts.it_value.tv_sec  = nanosecs / 1000000000;
	port->rdm_timer.ts.it_value.tv_nsec = nanosecs % 1000000000;
	port->rdm_timer.ts.it_interval.tv_sec  = 0;
	port->rdm_timer.ts.it_interval.tv_nsec = 0;

	// TODO: maybe we'll need to have an array of itimerspec
	status = timer_settime(port->rdm_timer.id, 0, &port->rdm_timer.ts, 0);
	if (status == -1)
	{
		printk (KERN_ERR"failed to adjust timer\n");
	}
}

void dmx512rtuart_periodical(struct rtuart * uart)
{
	struct dmx512_uart_port * port = rtuart_to_dmx512_uart_port(uart);
	if (port->rdm_timer.callTimeoutHandler) {
		printk (KERN_INFO"dmx512_rtuart_periodical do work\n");
		port->rdm_timer.callTimeoutHandler = 0;
		if (port->rdm_timer.func)
			port->rdm_timer.func(port);
	}
}

/*------ END-OF RDM TIMEOUT functions ------*/






static void dmxrtuart_enable_rs485_transmitter_rts (struct dmx512_uart_port * port,
						    const int en)
{
	if (en)
		rtuart_clr_control(port->uart, RTUART_CONTROL_RTS);
	else
		rtuart_set_control(port->uart, RTUART_CONTROL_RTS);
}


static void dmxrtuart_enable_rs485_transmitter_gpio (struct dmx512_uart_port * port,
						     const int en)
{
	if (port->gpio_dmxtxen.line) {
		if (gpiod_line_set_value(port->gpio_dmxtxen.line, en))
			printk (KERN_WARNING"failed to set direction gpio\n");
	}
}

static int dmxrtuart_rs485txen_init(struct dmx512_uart_port * port,
				    const char * rs485_io)
{
	port->gpio_dmxtxen.chip = 0;
	port->gpio_dmxtxen.line = 0;
	if (strcmp(rs485_io, "gpiochip0.18") == 0) {
		port->gpio_dmxtxen.chip = gpiod_chip_open_by_name("gpiochip0");
		if (port->gpio_dmxtxen.chip) {
			port->gpio_dmxtxen.line = gpiod_chip_get_line(port->gpio_dmxtxen.chip, 18);
			if (0 == port->gpio_dmxtxen.line) {
				gpiod_chip_close(port->gpio_dmxtxen.chip);
				port->gpio_dmxtxen.chip = 0;
				printk(KERN_ERR"!!!!!!! failed to get gpio0.18\n");
			}
			else if (gpiod_line_request_output(port->gpio_dmxtxen.line, "dmxtxen", 0)) {
				printk (KERN_ERR"!!!!!!! failed to request gpio0.18 as output\n");
				gpiod_line_release(port->gpio_dmxtxen.line);
				gpiod_chip_close(port->gpio_dmxtxen.chip);
				port->gpio_dmxtxen.line = 0;
				port->gpio_dmxtxen.chip = 0;
			}
		}
		else
			printk (KERN_ERR"!!!!!!! failed to create gpio0\n");

		if (port->gpio_dmxtxen.line) {
			port->enable_rs485_transmitter = dmxrtuart_enable_rs485_transmitter_gpio;
		}
	}
	else if (strcmp(rs485_io, "rts") == 0) {
		port->enable_rs485_transmitter = dmxrtuart_enable_rs485_transmitter_rts;
	}

	/* switch to input */
	if (port->enable_rs485_transmitter)
		port->enable_rs485_transmitter(port, 0);
}



static struct dmx512_framequeue_entry *
dmx512_rtuart_complete_rxframe(struct dmx512_uart_port * dmxport)
{
	const struct rtuart_buffer * dmx_rx_buffer = &dmxport->rx.buffer;
	struct dmx512_framequeue_entry * frame = dmxport->rx.frame;
	dmxport->rx.frame = 0;
	const int count = dmx_rx_buffer->transfered;
	frame->frame.payload_size = count - 1;
	return frame;
}


static void dmx512_rtuart_start_receive_data(struct dmx512_uart_port * dmxport,
					     const int buffer_offset,
					     const int startcode_trigger)
{
	rtuart_set_notify (dmxport->uart, RTUART_NOTIFY_RECEIVER_EVENT);
	rtuart_set_notify (dmxport->uart, RTUART_NOTIFY_RECEIVER_HASDATA);

	dmxport->rx.frame = dmx512_get_frame(&dmxport->dmx);
	if (dmxport->rx.frame) {
		dmxport->rx.buffer.data = dmxport->rx.frame->frame.data + buffer_offset;
		dmxport->rx.buffer.size = 513 - buffer_offset;
		dmxport->rx.buffer.validcount = dmxport->rx.buffer.size;
		if (startcode_trigger) {
			dmxport->rx.buffer.triggermask = 1;
			dmxport->rx.buffer.trigger[0] = 1;
		}
		rtuart_rx_start (dmxport->uart, &dmxport->rx.buffer);
	}
}



/*=========== UART Callbacks =================*/

static void dmx512_rtuart_tx_start (struct rtuart * uart, struct rtuart_buffer * buffer)
{
}

static void dmx512_rtuart_tx_end1 (struct rtuart * uart, struct rtuart_buffer * buffer)
{
	rtuart_set_notify (uart, RTUART_NOTIFY_TRANSMITTER_EMPTY);
}

static void dmx512_rtuart_tx_end2 (struct rtuart * uart, struct rtuart_buffer * buffer)
{
	struct dmx512_uart_port * dmxport = rtuart_to_dmx512_uart_port(uart);
	// TODO: may add a callback here, that the next frame can be transmitted.
	// TODO: add statemachine handling here

	switch (dmxport->state)
	{
	case PORT_STATE_TRANSMIT_DATA_ONEWAY:
		if (dmxport->enable_rs485_transmitter)
			dmxport->enable_rs485_transmitter(dmxport, 0);
		if (dmxport->tx.frame) {
			dmx512_put_frame(&dmxport->dmx, dmxport->tx.frame);
			dmxport->tx.frame = 0;
		}
		else
			printk(KERN_ERR"no tx-frame in tx_end2");
		rtuart_set_notify (uart, RTUART_NOTIFY_RECEIVER_EVENT);
		next_state_label(dmxport, PORT_STATE_IDLE, "tx_end2");
		break;

	case PORT_STATE_TRANSMIT_DATA_TWOWAY:
		if (dmxport->enable_rs485_transmitter)
			dmxport->enable_rs485_transmitter(dmxport, 0);
		if (dmxport->tx.frame) {
			struct dmx512_framequeue_entry * frame = dmxport->tx.frame;

			if (dmx512_is_rdm_disc_unique_branch_request(&frame->frame)) {
				next_state_label(dmxport, PORT_STATE_RECV_RDMDISC_REPLY, "tx_end2");
				dmx512_rtuart_start_receive_data(dmxport,
								 RDM_POS_DISC_UNIQUE_BANCH_DATA,
								 0);
			}

			else /*if (dmx512_is_rdm_request(&frame->frame))*/ {
				next_state_label(dmxport, PORT_STATE_EXPECT_RDM_REPLY_BREAK, "tx_end2");
				rtuart_set_notify (uart, RTUART_NOTIFY_RECEIVER_EVENT);
			}
		}
		else
			next_state_label(dmxport, PORT_STATE_IDLE, "tx_end2");
		break;

	default:
		printk(KERN_ERR"unexpected tx_end2 in state %s", statename(dmxport->state));
		break;
	}
}

static void dmx512_rtuart_input_change (struct rtuart * uart,
			       unsigned long eventmask,
			       unsigned long values)
{
	printk (KERN_INFO"rtuart_dev_input_change(mask:%08lX  val:%08lX)\n", eventmask, values);
}

static void dmx512_rtuart_discovery_reply(struct dmx512_uart_port * dmxport,
					  const u8 * src,
					  const u8 ack)
{
	// TODO: implement
	struct dmx512_framequeue_entry * txframe = dmxport->tx.frame;
	struct dmx512_framequeue_entry * rxframe = dmxport->rx.frame;
	dmxport->tx.frame = 0;
	dmxport->rx.frame = 0;

	memcpy (txframe->frame.data + RDM_POS_DEST_UID,
		txframe->frame.data + RDM_POS_SRC_UID,
		6);
	memcpy (txframe->frame.data+RDM_POS_SRC_UID, src, 6);
	txframe->frame.data[RDM_POS_RESPONSE_TYPE] = ack;
	txframe->frame.data[RDM_POS_COMMAND_CLASS] = DISCOVERY_COMMAND_RESPONSE;

	dmx512_put_frame(&dmxport->dmx, rxframe);
	dmx512_received_frame(&dmxport->dmx, txframe);
}

static void dmx512_rtuart_discovery_reply_collision(struct dmx512_uart_port * dmxport)
{
	const u8 src[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
	dmx512_rtuart_discovery_reply(dmxport,
				      src,
				      RESPONSE_TYPE_ACK);
}

static void dmx512_rtuart_discovery_reply_timeout(struct dmx512_uart_port * dmxport)
{
	const u8 src[6] = {0,0,0,0,0,0};
	dmx512_rtuart_discovery_reply(dmxport,
				      src,
				      RESPONSE_TYPE_NOREPLY);
}

static void dmx512_rtuart_handle_discovery_reply(struct dmx512_uart_port * dmxport)
{
	printf("dmx512_rtuart_handle_discovery_reply: state=%s transfered=%d\n",
	       statename(dmxport->state),
	       dmxport->rx.buffer.transfered);
	if (dmxport->rx.buffer.transfered > 0)
	{
		int i;
		for (i = 0; i < dmxport->rx.buffer.transfered; ++i)
			printf (" %02X",
				dmxport->rx.frame->frame.data[i+RDM_POS_DISC_UNIQUE_BANCH_DATA]);
		printf ("\n");
	}

	if (dmxport->state == PORT_STATE_RECV_RDMDISC_REPLY_COLLISION)
	{
		dmx512_rtuart_discovery_reply_collision(dmxport);
		return;
	}

	if (dmxport->rx.buffer.transfered == 0) {
		dmx512_rtuart_discovery_reply_timeout(dmxport);
		return;
	}

	// we expected a discovery reply and it somehow is completed.
	unsigned char tmp[8];
	const unsigned char * reply_data = dmxport->rx.frame->frame.data + RDM_POS_DISC_UNIQUE_BANCH_DATA;

	int j = 0;

	while ((*reply_data != 0xFE) && (j < dmxport->rx.buffer.transfered)) {
		++reply_data;
		++j;
	}

	while ((*reply_data == 0xFE) && (j < dmxport->rx.buffer.transfered)) {
		++reply_data;
		++j;
	}
	if (*reply_data != 0xAA)
	{
		dmx512_rtuart_discovery_reply_collision(dmxport);
		return;
	}
	++reply_data;
	++j;
	printk(KERN_NOTICE"RDM-Discovery-Reply: preamble-ok(%d)\n", j);

	if (dmxport->rx.buffer.transfered < (j+16)) {
		dmx512_rtuart_discovery_reply_collision(dmxport);
		printk(KERN_NOTICE"RDM-Discovery-Reply: payload is to small\n");
		return;
	}

	const u16 calc_checksum = dmx512frame_calculate_rdm_checksum(reply_data, 12);
	int i;
	for (i = 0; i < 8; ++i, reply_data += 2)
	{
		if ((reply_data[0] & 0xAA) != 0xAA) {
			dmx512_rtuart_discovery_reply_collision(dmxport);
			return;
		}
		if ((reply_data[1] & 0x55) != 0x55) {
			dmx512_rtuart_discovery_reply_collision(dmxport);
			return;
		}
		tmp[i] = reply_data[0] & reply_data[1];
	}
	u16 rx_checksum = (tmp[6]<<8) | tmp[7];
	if (rx_checksum != calc_checksum) {
		dmx512_rtuart_discovery_reply_collision(dmxport);
		return;
	}

	dmx512_rtuart_discovery_reply(dmxport,
				      tmp,
				      RESPONSE_TYPE_ACK);
}


static void dmx512_rtuart_build_rdm_reply_timeout(struct dmx512_framequeue_entry * frame)
{
	// TODO: change the frame to be a reply with timeout.
}

static void dmx512_rtuart_handle_rdm_reply_timeout(struct dmx512_uart_port * dmxport)
{
	// TODO: implement dmx512_rtuart_handle_rdm_reply_timeout

	struct dmx512_framequeue_entry * txframe = dmxport->tx.frame;
	struct dmx512_framequeue_entry * rxframe = dmxport->rx.frame;
	dmxport->tx.frame = 0;
	dmxport->rx.frame = 0;

	dmx512_put_frame(&dmxport->dmx, rxframe);

	dmx512_rtuart_build_rdm_reply_timeout(txframe);
	dmx512_received_frame(&dmxport->dmx, txframe);
}



static void dmx512_rtuart_rx_err (struct rtuart * uart, unsigned long errmask)
{
	if (errmask & RTUART_EVENT_OVERRUN_ERROR)
		printf("RX-OVERRUN-ERROR\n");

	if (errmask & RTUART_EVENT_BREAK) {
		struct dmx512_uart_port * dmxport = rtuart_to_dmx512_uart_port(uart);

		switch(dmxport->state)
		{
		case PORT_STATE_DOWN:
			// do nothing as the port is not yet up.
			break;

		case PORT_STATE_IDLE:
		case PORT_STATE_IDLE_AFTER_DMXDATA00:
			// 1. goto state EXPECT_STARTCODE
			// 2. start receive data.
			next_state_label(dmxport, PORT_STATE_EXPECT_STARTCODE, "received break");
			dmx512_rtuart_start_receive_data(dmxport, 0, 1);
			break;

		case PORT_STATE_EXPECT_RDM_REPLY_BREAK:
			// 1. goto state RECV_RDM_REPLY_DATA
			// 2. and start receive data.
			next_state_label(dmxport, PORT_STATE_RECV_RDM_REPLY_DATA, "received break");
			dmx512_rtuart_start_receive_data(dmxport, 0, 1);
			break;

		case PORT_STATE_EXPECT_STARTCODE:
			// not invalid - two consecutive breaks are uncommon but allowed.
			// do nothing, as everything is allready setup.
			break;
		case PORT_STATE_RECEIVE_DMXDATA:
			// this means that the former dmx-frame is complete.
			// 1. complete currently received frame,
			//    and call dmx-statistic function,
			// 2. goto state RECEIVE_DMXDATA and
			// 3. start receive data.
			if (dmxport->rx.frame) {
				rtuart_rx_stop (uart, &dmxport->rx.buffer);
				struct dmx512_framequeue_entry * frame = dmx512_rtuart_complete_rxframe(dmxport);
				if (frame->frame.startcode == 0)
					slot_count_lock__received_frame(&dmxport->slot_count_lock, &frame->frame);
				dmx512_received_frame(&dmxport->dmx, frame);
			}
			// call dmx stats
			next_state_label(dmxport, PORT_STATE_EXPECT_STARTCODE, "received break");
			dmx512_rtuart_start_receive_data(dmxport, 0, 1);
			break;

		case PORT_STATE_TRANSMIT_DATA_ONEWAY:
		case PORT_STATE_TRANSMIT_DATA_TWOWAY:
		case PORT_STATE_RECV_RDM_REPLY_DATA:
			// unexpected DMX512-Break
			break;

		case PORT_STATE_TRANSMIT_BREAK_ONEWAY:
		case PORT_STATE_TRANSMIT_BREAK_TWOWAY:
			// maybe there is a physical loop that can not be disabled.
			// this is not critical.
			break;

		case PORT_STATE_RECV_RDMDISC_REPLY:
			// RDM discovery reply collision
			// signal collision
			// somehow indicate a collision has beed received.
			next_state_label(dmxport, PORT_STATE_RECV_RDMDISC_REPLY_COLLISION, "received break");
			break;

		case PORT_STATE_RECV_RDMDISC_REPLY_COLLISION:
			// we allready received a DMX512-Break so lets ignore it.
			break;

		default:
			// illegal/unknown state
			printk(KERN_ERR"received dmx512-break in illegal state %d\n", dmxport->state);
			break;
		}
	}
}

static void dmx512_rtuart_rx_char (struct rtuart * uart, const unsigned short c)
{
	/*
	 * If we received data without a buffer allocated and
	 * the slot count we locked on is less than the allowed 512 slots
	 * we lost the lock. If we received more data than the 512 slots,
	 * it must be garbage from the console and we just ignore it for now.
	 * Maybe that is also handled as junk somehow.
	 *
	 * TODO: We add another state RECEIVED_DATA00 which is entered
	 * from RECEIVE_DATA instead of IDLE if startcode was 00. It has
	 * the same outputs as IDLE but rx_char. if rx_char is received
	 * during RECEIVED_DATA00 it is clear that more data has been received
	 * than expected and the slotcount-lock opens up (unlocks).
	 */
	struct dmx512_uart_port * dmxport = rtuart_to_dmx512_uart_port(uart);
	if (dmxport->state == PORT_STATE_IDLE_AFTER_DMXDATA00) {
		if (dmxport->slot_count_lock.matching_slot_count < 512) {
			slot_count_lock__lost_lock(&dmxport->slot_count_lock);
		}
		next_state_label(dmxport, PORT_STATE_IDLE, "rx_char");
	}
}


static void dmx512_rtuart_rx_end (struct rtuart * uart, struct rtuart_buffer * buffer)
{
	printk (KERN_INFO"test_rx_end(buffer:%p)\n", buffer);

	struct dmx512_uart_port * dmxport = rtuart_to_dmx512_uart_port(uart);
	if (dmxport->rx.frame) {
		switch (dmxport->state)
		{
		case PORT_STATE_DOWN:
		case PORT_STATE_IDLE:
		case PORT_STATE_IDLE_AFTER_DMXDATA00:
		case PORT_STATE_EXPECT_STARTCODE:
		case PORT_STATE_TRANSMIT_BREAK_ONEWAY:
		case PORT_STATE_TRANSMIT_DATA_ONEWAY:
		case PORT_STATE_TRANSMIT_BREAK_TWOWAY:
		case PORT_STATE_TRANSMIT_DATA_TWOWAY:
		case PORT_STATE_EXPECT_RDM_REPLY_BREAK:
			printk(KERN_ERR"unexpected rx_end event\n");
			/* do nothing, this should not happen */
			break;

		case PORT_STATE_RECEIVE_DMXDATA:
			if (dmxport->rx.frame->frame.startcode == 0) {
				// enable-dmx0-timeout-timer of one second and after timeout change state to PORT_STATE_IDLE and unloch dmx-slot-count-lock
				next_state_label(dmxport, PORT_STATE_IDLE_AFTER_DMXDATA00, "rx_end");
			}
			else
				next_state_label(dmxport, PORT_STATE_IDLE, "rx_end");

			dmx512_received_frame(&dmxport->dmx, dmx512_rtuart_complete_rxframe(dmxport));
			break;

		case PORT_STATE_RECV_RDM_REPLY_DATA:
			if (dmxport->tx.frame) {
				dmx512_put_frame(&dmxport->dmx, dmxport->tx.frame);
				dmxport->tx.frame = 0;
			}
			dmx512_received_frame(&dmxport->dmx, dmx512_rtuart_complete_rxframe(dmxport));
			next_state_label(dmxport, PORT_STATE_IDLE, "rx_end");
			break;

		case PORT_STATE_RECV_RDMDISC_REPLY:
		case PORT_STATE_RECV_RDMDISC_REPLY_COLLISION:
			dmx512rtuart_stop_rdm_timer(dmxport);
			dmx512_rtuart_handle_discovery_reply(dmxport);
			next_state_label(dmxport, PORT_STATE_IDLE, "rx_end");
			break;
		}
	}
}

static void dmx512_rtuart_rx_timeout (struct rtuart * uart, struct rtuart_buffer * buffer)
{
	return;
	printk (KERN_INFO"test_rx_timeout(buffer:%p)  %lu\n",
		buffer,
		buffer ? buffer->transfered : -1
		);
}

static void dmx512_rtuart_rx_trigger(struct rtuart * uart,
		     struct rtuart_buffer * buffer,
		     const unsigned long triggermask
		     )
{
	struct dmx512_uart_port * dmxport = rtuart_to_dmx512_uart_port(uart);

	printk (KERN_INFO"test_rx_trigger(buffer:%p, triggermask=%08lX)\n",
		buffer,
		triggermask);

	if (triggermask & 1) {
		switch(dmxport->state)
		{
		case PORT_STATE_EXPECT_STARTCODE:
			switch (buffer->data[0])
			{
			case SC_RDM:
				printk (KERN_INFO"RDM received\n");
				if (buffer->transfered > RDM_POS_LENGTH) {
					if (buffer->data[RDM_POS_SUBSTARTCODE] == SC_SUB_MESSAGE)
						buffer->validcount = buffer->data[RDM_POS_LENGTH] + 2;
					printk (KERN_INFO"RDM length @0 %d\n", buffer->validcount);
				}
				else {
					buffer->triggermask |= 2;
					buffer->trigger[1] = RDM_POS_LENGTH;
					printk (KERN_INFO"RDM trigger for length @ %d\n", buffer->trigger[1]);
				}
				break;

			case 0x00:
				if (dmxport->slot_count_lock.locked) {
					buffer->validcount = dmxport->slot_count_lock.matching_slot_count + 1;
				}
				break;
			}
			next_state_label(dmxport, PORT_STATE_RECEIVE_DMXDATA, "rxtrigger0");
			break;

		case PORT_STATE_RECV_RDM_REPLY_DATA:
			if (buffer->data[0]==SC_RDM) {
				printk (KERN_INFO"RDM received\n");
				if (buffer->transfered > RDM_POS_LENGTH) {
					if (buffer->data[RDM_POS_SUBSTARTCODE] == SC_SUB_MESSAGE)
						buffer->validcount = buffer->data[RDM_POS_LENGTH] + 2;
					printk (KERN_INFO"RDM length @0 %d\n", buffer->validcount);
				}
				else {
					buffer->triggermask |= 2;
					buffer->trigger[1] = RDM_POS_LENGTH;
					printk (KERN_INFO"RDM trigger for length @ %d\n", buffer->trigger[1]);
				}
			}
			break;

		default:
			break;
		}
	}

	if (triggermask & 2) {
		if ((dmxport->state == PORT_STATE_RECEIVE_DMXDATA) ||
		    (dmxport->state == PORT_STATE_RECV_RDM_REPLY_DATA))
		{
			if (buffer->transfered > RDM_POS_LENGTH) {
				if (buffer->data[RDM_POS_SUBSTARTCODE] == SC_SUB_MESSAGE)
					buffer->validcount = buffer->data[RDM_POS_LENGTH] + 2;
				printk (KERN_INFO"RDM length @1 %d\n", buffer->validcount);
#if 0
				int i;
				for (i = 0; i < buffer->transfered; ++i)
					printf (" %02X", buffer->data[i]);
				printf ("\n");
#endif
			}
		}
	}

}


static void dmx512rtuart_handle_rdm_timeout(struct dmx512_uart_port * dmxport)
{
	/*
	 * We only handle timeouts for replies for which we have a request.
	 */
	if (!dmxport->tx.frame)
		return;
	dmx512rtuart_stop_rdm_timer(dmxport);

	printk (KERN_DEBUG"dmx512rtuart_handle_rdm_timeout(%p)\n %s\n",
		dmxport,
		statename(dmxport->state));

	switch (dmxport->state)
	{
	case PORT_STATE_RECV_RDMDISC_REPLY:
	case PORT_STATE_RECV_RDMDISC_REPLY_COLLISION:
		dmx512_rtuart_handle_discovery_reply(dmxport);
		next_state_label(dmxport, PORT_STATE_IDLE, "rdm-timeout");
		break;

	case PORT_STATE_EXPECT_RDM_REPLY_BREAK:
		dmx512_rtuart_handle_rdm_reply_timeout(dmxport);
		next_state_label(dmxport, PORT_STATE_IDLE, "rdm-timeout");
		break;
	case PORT_STATE_RECV_RDM_REPLY_DATA:
		dmx512_rtuart_handle_rdm_reply_timeout(dmxport);
		next_state_label(dmxport, PORT_STATE_IDLE, "rdm-timeout");
		break;

	case PORT_STATE_DOWN:
	case PORT_STATE_IDLE:
	case PORT_STATE_IDLE_AFTER_DMXDATA00:
	case PORT_STATE_EXPECT_STARTCODE:
	case PORT_STATE_RECEIVE_DMXDATA:
	case PORT_STATE_TRANSMIT_BREAK_ONEWAY:
	case PORT_STATE_TRANSMIT_DATA_ONEWAY:
	case PORT_STATE_TRANSMIT_BREAK_TWOWAY:
	case PORT_STATE_TRANSMIT_DATA_TWOWAY:
	default:
		printk (KERN_DEBUG"unexpected rdm timeout in state %s\n", statename(dmxport->state));
		break;
	}
}



struct rtuart_callbacks dmx512_rtuart_callbacks =
{
	.tx_start     = dmx512_rtuart_tx_start,
	.tx_end1      = dmx512_rtuart_tx_end1,
	.tx_end2      = dmx512_rtuart_tx_end2,
	.input_change = dmx512_rtuart_input_change,
	.rx_err       = dmx512_rtuart_rx_err,
	.rx_char      = dmx512_rtuart_rx_char,
	.rx_end       = dmx512_rtuart_rx_end,
	.rx_timeout   = dmx512_rtuart_rx_timeout,
	.rx_trigger   = dmx512_rtuart_rx_trigger
};


static void dmx512_rtuart_start_frame (struct dmx512_uart_port * dmxport,
				       struct dmx512_framequeue_entry * frame
	)
{
	rtuart_clr_notify (dmxport->uart, RTUART_NOTIFY_RECEIVER_EVENT | RTUART_NOTIFY_RECEIVER_HASDATA);

	// claim dmx-port
	dmxport->tx.frame = frame;
	dmxport->tx.buffer.data = dmxport->tx.frame->frame.data;
	dmxport->tx.buffer.size = dmxport->tx.frame->frame.payload_size + 1;
	dmxport->tx.buffer.validcount = dmxport->tx.buffer.size;
	dmxport->tx.buffer.triggermask = 0;

	// check if this is a oneway:
	//    DMX00, rdm-reply, rdm-discovery-reply, rdm-request with dest=broadcast
	// check if this is a twoway:
	//    rdm-discovery-request, rdm-request with dest!=broadcast

	const int twoway =
		dmx512_is_rdm_disc_unique_branch_request(&frame->frame) ||
		( dmx512_is_rdm_request(&frame->frame) &&
		  dmx512_rdm_frame_dest(&frame->frame) != 0xFFFFFFFFFFFFLL
			);

	if (dmx512_is_rdm_frame(&frame->frame)) {
		if (dmx512_is_rdm_disc_unique_branch_request(&frame->frame)) {
			dmx512rtuart_start_rdm_timer(dmxport, TIMEOUT_RDMDISCOVERY_LENGTH);
		}
		else if (dmx512_is_rdm_request(&frame->frame)) {
			dmx512rtuart_start_rdm_timer(dmxport, DISCOVERY_REQUEST_REPLY_TIMEOUT);
		}
		else if (dmx512_is_rdm_disc_unique_branch_reply(&frame->frame)) {
			frame->frame.flags |= DMX512_FLAG_NOBREAK;
			dmxport->tx.buffer.data += RDM_POS_DISC_UNIQUE_BANCH_DATA;
			dmxport->tx.buffer.size = dmx512_rdm_build_rdmdiscovery_reply(
				dmxport->tx.buffer.data,
				&dmxport->tx.frame->frame);
			dmxport->tx.buffer.validcount = dmxport->tx.buffer.size;
#if 0
			printf ("DISC_REPLY:");
			int i;
			for (i = 0; i < dmxport->tx.buffer.size; ++i)
				printf (" %02X", dmxport->tx.buffer.data[i]);
			printf ("\n");
			/* no timeout handling needed. */
#endif
		}
		else {
			// must be an rdm reply. We do not need special handling.
		}
	}

	next_state_label(dmxport,
			 twoway ? PORT_STATE_TRANSMIT_BREAK_TWOWAY
			        : PORT_STATE_TRANSMIT_BREAK_ONEWAY,
			 "start_frame");

	if (dmxport->enable_rs485_transmitter)
		dmxport->enable_rs485_transmitter(dmxport, 1);

	if (0 == (frame->frame.flags & DMX512_FLAG_NOBREAK)) {
		rtuart_set_control(dmxport->uart, RTUART_CONTROL_BREAK);
		usleep(88);
		rtuart_clr_control(dmxport->uart, RTUART_CONTROL_BREAK);
	}

	if (dmxport->state == PORT_STATE_TRANSMIT_BREAK_TWOWAY) {
		next_state_label(dmxport, PORT_STATE_TRANSMIT_DATA_TWOWAY, "start_frame");
		// start rdm reply timeout
	}
	else if (dmxport->state == PORT_STATE_TRANSMIT_BREAK_ONEWAY) {
		next_state_label(dmxport, PORT_STATE_TRANSMIT_DATA_ONEWAY, "start_frame");
	}
	rtuart_tx_start (dmxport->uart, &dmxport->tx.buffer);
}


static int dmx512_rtuart_client_send_frame (struct dmx512_port * dmxport, struct dmx512_framequeue_entry * frame)
{
	struct dmx512_uart_port * port = dmx512port_to_dmx512_uart_port(dmxport);
	if (port->tx.frame)
		return -1;

	printk(KERN_INFO"dmx512_rtuart_client_send_frame\n");

	switch (port->state)
	{
	case PORT_STATE_TRANSMIT_BREAK_ONEWAY:
	case PORT_STATE_TRANSMIT_DATA_ONEWAY:
	case PORT_STATE_TRANSMIT_BREAK_TWOWAY:
	case PORT_STATE_TRANSMIT_DATA_TWOWAY:
	case PORT_STATE_RECV_RDMDISC_REPLY:
	case PORT_STATE_RECV_RDMDISC_REPLY_COLLISION:
	case PORT_STATE_EXPECT_RDM_REPLY_BREAK:
	case PORT_STATE_RECV_RDM_REPLY_DATA:
		printk("send_frame port is busy\n");
		return -1;

	case PORT_STATE_DOWN:
	case PORT_STATE_IDLE:
	case PORT_STATE_IDLE_AFTER_DMXDATA00:
	case PORT_STATE_EXPECT_STARTCODE:
	case PORT_STATE_RECEIVE_DMXDATA:
	default:
		dmx512_rtuart_start_frame (port, frame);
		break;
	}
	return 0;
}

struct dmx512_port * dmx512_rtuart_client_create(struct rtuart * uart,
						 const char * rs485_io)
{
	struct dmx512_uart_port * port = malloc(sizeof(*port));
	bzero(port, sizeof(*port));

	/* initialize dmx-port */
	strcpy(port->dmx.name, "uio-dmx");
	port->dmx.capabilities = 0; // DMX512_CAP_RDM
	port->dmx.send_frame = dmx512_rtuart_client_send_frame;
	port->rdm_timer.func = dmx512rtuart_handle_rdm_timeout;
	slot_count_lock__init(&port->slot_count_lock);

	/* initialize uart */
	port->uart = uart;
	rtuart_assign_callbacks(uart, &dmx512_rtuart_callbacks, port);
	rtuart_set_baudrate(uart, 250000); // 
	rtuart_set_format(uart, 8,'n', 2); // "8n2"
	rtuart_set_handshake(uart, RTUART_HANDSHAKE_OFF);
	rtuart_clr_control(uart, RTUART_CONTROL_LOOP); // disable loop
	rtuart_set_notify (uart, RTUART_NOTIFY_RECEIVER_EVENT);

	dmxrtuart_rs485txen_init(port, rs485_io);
	next_state_label(port, PORT_STATE_IDLE, "port is up");
	port->slot_count_lock.enabled = 1;

	return &port->dmx;
}


/*----------------------------------------------*/

void dump_dmx512_frame(struct dmx512_framequeue_entry * frame, const char * prompt)
{
	printf ("%s frame:%p port:%d break:%d size:%d sc:%02X",
		prompt,
		frame,
		frame->frame.port,
		frame->frame.breaksize,
		frame->frame.payload_size,
		frame->frame.startcode
		);
	int i;
	for (i = 0; i < frame->frame.payload_size && i < 64; ++i)
	{
		if (i%16 == 0)
			printf ("\n%03X :", i);
		printf (" %02X", frame->frame.payload[i]);
	}
	if (i < frame->frame.payload_size)
		printf ("\n .....");
	printf ("\n");
}


struct dmx512_framequeue_entry * dmx512_get_frame(struct dmx512_port *port)
{
	(void)port;
	struct dmx512_framequeue_entry * f = (struct dmx512_framequeue_entry *)malloc(sizeof(*f));
	if (f)
		memset(f, 0, sizeof(*f));
	return f;
}

void dmx512_put_frame(struct dmx512_port *port, struct dmx512_framequeue_entry * frame)
{
	(void)port;
	if (frame)
		free(frame);
}

int dmx512_send_frame (struct dmx512_port * port, struct dmx512_framequeue_entry * frame)
{
	if (port->send_frame)
		return port->send_frame (port, frame);
	return -1;
}

struct dmx512_framequeue_entry * received_frames = 0;
static int received_dmx_signal = -1;
static unsigned long ok_counter = 0;
int dmx512_received_frame(struct dmx512_port *port, struct dmx512_framequeue_entry * frame)
{
	if (!frame)
		return 0;
	frame->head = received_frames;
	received_frames = frame;
	write(received_dmx_signal, "", 1);
	return 0;
}


/*-------------------------------------------------------*/




/* call with: chrt -r -p 99 <pid> */
#include <signal.h>

static volatile int g_run = 0;
static void handle_stop(int sig)
{
	g_run = 0;
}

int main (int argc, char ** argv)
{
	if (argc <= 2) {
		fprintf(stderr, "usage: %s [pc16c550|pl011] <ui0-device> [--debug|--info]\n", argv[0]);
		return 0;
	}

	if (argc > 3) {
		if (strcmp(argv[3], "--debug")==0)
			set_loglevel(LOGLEVEL_DEBUG);
		else if (strcmp(argv[3], "--info")==0)
			set_loglevel(LOGLEVEL_INFO);
		else if (strcmp(argv[3], "--notice")==0)
			set_loglevel(LOGLEVEL_NOTICE);
	}
	const char * uart_type = argv[1];
	const char * uio_device = argv[2];

	struct rtuart * uart = rtuart_create(uart_type, uio_device);
	if (uart == 0) {
		fprintf(stderr, "failed to create uart\n");
		fprintf(stderr, "usage: %s [pc16c550|pl011] <ui0-device>\n", argv[0]);
		return 1;
	}
	struct dmx512_port * dmxport = dmx512_rtuart_client_create(
		uart,
		(strcmp(uart_type, "pl011")==0) ? "gpiochip0.18" : "rts"
		);

	signal(SIGHUP, handle_stop);
	signal(SIGINT, handle_stop);
	signal(SIGQUIT, handle_stop);
	g_run = 1;

	const char * dmx_cardname = "/dev/dmx-card1";
	int dmxfd = open(dmx_cardname, O_RDWR | O_NONBLOCK);
	if (dmxfd < 0)
		return 1;
	const int dmxport_id = 0;

	int rxdmx_pipe[2];
	if (pipe(rxdmx_pipe))
	  {
	    perror("pipe");
	    exit(1);
	  }
	received_dmx_signal = rxdmx_pipe[1];
	while(g_run)
	{
		struct pollfd fds[2];
		memset (fds, 0, sizeof (fds));
		fds[0].fd = dmxfd;
		fds[0].events = POLLIN;

		fds[1].fd = rxdmx_pipe[0];
		fds[1].events = POLLIN;
		const int ret = poll(fds, sizeof(fds)/sizeof(fds[0]), 1);

		if (ret < 0)
			return ret;
		else if (ret == 0) {
			// handle timeouts
			dmx512rtuart_periodical(uart);
			continue;
		}

		if (fds[0].revents == POLLIN) {
			struct dmx512_framequeue_entry * frame = dmx512_get_frame(dmxport);
			if (frame) {
				struct dmx512frame * f = &frame->frame;
				const int n = read (dmxfd, f, sizeof(*f));
				if ((n == sizeof(*f)) && (f->port == 0)) {
					const int ret = dmx512_send_frame (dmxport, frame);
					if (ret < 0)
						dmx512_put_frame(dmxport, frame);
				}
				else
					dmx512_put_frame(dmxport, frame);
			}
		}

		if (fds[1].revents == POLLIN) {
			char c;
			int n = read (rxdmx_pipe[0], &c, sizeof(c));
			// if (n > 0) printf ("received-dmx-frame\n");

			struct dmx512_framequeue_entry * f = received_frames;
			received_frames = 0;

			while (f)
			{
				struct dmx512_framequeue_entry * next = f->head;
				f->head = 0;
			  
				const int n = write (dmxfd, &f->frame, sizeof(f->frame));
				if (n == sizeof(f->frame))
				{
					/*
					  printf ("send one frame to loopback port_%u size:%d\n",
					  f->frame.port,
					  f->frame.payload_size
					  );
					*/
					dmx512_put_frame(dmxport, f);
				}
				f = next;
			}
		}

		dmx512rtuart_periodical(uart);

	}

	printf ("Stopping\n");
	rtuart_clr_notify (uart, RTUART_NOTIFY_RECEIVER_EVENT);

	rtuart_cleanup(uart);
	printf ("\n\n\n");

	return 0;
}
