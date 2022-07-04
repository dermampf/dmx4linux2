// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#include "rtuart_dmx512_client.h"
#include "rtuart_client.h"
#include "rtuart.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <signal.h> // for posix timer functions.
#include <time.h> // for posix timer functions.

#include <linux/dmx512/dmx512frame.h>
#include "dmx512_port.h"

extern void dump_dmx512_frame(struct dmx512_framequeue_entry * frame, const char * prompt);


static void printlog(const char * msg)
{
  struct timespec now;
  if (0==clock_gettime(CLOCK_MONOTONIC, &now))
    printf ("[%lu.%09lu]%s", now.tv_sec, now.tv_nsec, msg);
  else
    printf ("[???]%s", msg);
}

enum {
	DISCOVERY_REQUEST_REPLY_TIMEOUT = 5000, /*us*/
	TIMEOUT_RDMDISCOVERY_LENGTH = 5000,  /* we expect all rdm discovery reply data to be received then */

	TIMEOUT_RDMFRAME_PEROCTET = 60,
	TIMEOUT_RDMFRAME_STATIC = 3000,
	TIMEOUT_RDM_EOP_TO_SOP = 5000, /* end of request to start of reply */

	/* start of packet to end of the same packet */
	TIMEOUT_RDM_SOP_TO_EOP = 3000 + TIMEOUT_RDMFRAME_PEROCTET*256,
};

enum dmx512_uart_state {
	PORT_STATE_DOWN = 0, // just constructed, not yet up.
	PORT_STATE_IDLE,

	PORT_STATE_FIRST_RXSTATE = 0x100,
	PORT_STATE_EXPECT_STARTCODE = PORT_STATE_FIRST_RXSTATE, // decide wether to receive RDM or DMX-Data
	PORT_STATE_RECEIVE_DMXDATA,

	PORT_STATE_EXPECT_RDM_FRAME, // After transmitting an rdm frame wait for the break.
	PORT_STATE_RECEIVE_RDMDATA,  // now receive data, but it should be an rdm frame.

	PORT_STATE_RDMDISC_RECV_PREAMBLE,
	PORT_STATE_RDMDISC_RECV_DATA,
	PORT_STATE_LAST_RXSTATE,

	
	PORT_STATE_FIRST_TXSTATE = 0x200,
	PORT_STATE_TRANSMIT_BREAK = PORT_STATE_FIRST_TXSTATE,
	PORT_STATE_TRANSMIT_DATA,
	PORT_STATE_WAIT_FOR_TX_COMPLETE, // switch to input
	PORT_STATE_LAST_TXSTATE,

	PORT_STATE_COUNT
};


static const char * statename (const enum dmx512_uart_state state)
{
	switch(state)
	{
	case PORT_STATE_DOWN:                  return "DOWN";
	case PORT_STATE_IDLE:                  return "IDLE";
	case PORT_STATE_EXPECT_STARTCODE:      return "EXPECT_STARTCODE";
	case PORT_STATE_RECEIVE_DMXDATA:       return "RECEIVE_DMXDATA";
	case PORT_STATE_EXPECT_RDM_FRAME:      return "EXPECT_RDM_FRAME";
	case PORT_STATE_RECEIVE_RDMDATA:       return "RECEIVE_RDMDATA";
	case PORT_STATE_RDMDISC_RECV_PREAMBLE: return "RDMDISC_RECV_PREAMBLE";
	case PORT_STATE_RDMDISC_RECV_DATA:     return "RDMDISC_RECV_DATA";
	case PORT_STATE_LAST_RXSTATE:          return "LAST_RXSTATE";
	case PORT_STATE_TRANSMIT_BREAK:        return "TRANSMIT_BREAK";
	case PORT_STATE_TRANSMIT_DATA:         return "TRANSMIT_DATA";
	case PORT_STATE_WAIT_FOR_TX_COMPLETE:  return "WAIT_FOR_TX_COMPLETE";
	case PORT_STATE_LAST_TXSTATE:          return "LAST_TXSTATE";
	default:
		break;
	}
	return "???";
};


#if 1
#define next_state(p,s)  { printf("change state from %s to %s in %s\n", statename ((p)->state), statename (s), __func__); (p)->state = s; }
#define next_state_label(p,s,l)  { printf("change state from %s to %s in %s:%s\n", statename ((p)->state), statename (s), __func__, l); (p)->state = s; }
#else
#define next_state(p,s)          { (p)->state = s; }
#define next_state_label(p,s,l)  { (p)->state = s; }
#endif

struct dmx512_uart_port
{
	struct rtuart_client client;
	struct rtuart * uart;
	struct dmx512_port dmx;
	enum dmx512_uart_state state;
	void (*enable_rs485_transmitter)(struct dmx512_uart_port * port, const int on);

	struct {
		struct dmx512_framequeue_entry * frame;
		u8 * data;
		u32  count;
		struct {
			u32  enabled : 1;     /* the slot count lock is enabled */
			u32  locked  : 1;      /* the slot count is locked in */
			u32  required_count : 6;  /* The number of slots that are required for the lock to snap in */
			u32  matching_frame_count : 6; /* The number of currently valid frames */
			u32  matching_slot_count : 10; /* The slot count that has matched for the last <matching_frame_count> frames */
		} slot_count_lock;
		/* @expected_slots_count is set to matching_slot_count if locked is 1, else it is set to 512 for dmx frames and to the expected rdm size once the size of an RDM frame is detected. It does NOT include the startcode. */
		u16 expected_slots_count;
	} rx;
	
	struct {
		struct dmx512_framequeue_entry * frame;
		u8 * ptr;
		u32  remain;
	} tx;


	struct {
		int     active;
		timer_t id;
		struct sigevent se;
		struct itimerspec ts;
		int callTimeoutHandler;
	} rdm_timer;

};

// these two functions are for typesafety and for ease of use.
static inline struct dmx512_uart_port * rtuart_to_dmx512_uart_port(struct rtuart *uart)
{
	return container_of (uart->client, struct dmx512_uart_port, client);
}
static inline struct dmx512_uart_port * dmx512port_to_dmx512_uart_port(struct dmx512_port *dmx)
{
	return container_of(dmx, struct dmx512_uart_port, dmx);
}




static u16  dmx512frame_calculate_rdm_checksum(const u8 * data, const int size);
static void dmx512_uart_update_automatic_framecount_locking(struct dmx512_uart_port * port);
static struct dmx512_framequeue_entry* dmx512rtuart_checkout_tx_frame(struct dmx512_uart_port * port);
static struct dmx512_framequeue_entry* dmx512rtuart_checkout_rx_frame(struct dmx512_uart_port * port);








/*
Slot # Description             Data Value                   Remarks
0      START Code              SC_RDM
1      Sub-START Code          SC_SUB_MESSAGE
2      Message Length          25 (Slot # of Checksum High) Range 24 to 255.
3-8    Destination UID         0x123456789ABC
9 - 14 Source UID              0xCBA987654321
15     Transaction Number (TN) 0
16     Port ID / Response Type 0
17     Message Count           0
18 -   Sub-Device              0
19
20     Command Class (CC)      GET_COMMAND
21 -   Parameter ID (PID)      STATUS_MESSAGES
22
23     Parameter Data Length   1                            Range 0 to 231
       (PDL)
24     Parameter Data (PD)     STATUS_ERROR
25     Checksum High
26     Checksum Low

  Total Packet Time (n = number of data slots [1-513]) : 440uS +(n x 44uS ) + ((n -1) x 76uS )
  Inter Slot Time (Max): 2.0ms(TX)  2.1ms(RX)

*/
enum // RDM
{
	SC_RDM = 0xCC,
	SC_SUB_MESSAGE = 0x01,
	
	/* Table A-1: Command Class Defines
	 * RDM Command Classes (Slot 20) */
	DISCOVERY_COMMAND = 0x10,
	DISCOVERY_COMMAND_RESPONSE = 0x11,
	GET_COMMAND = 0x20,
	GET_COMMAND_RESPONSE = 0x21,
	SET_COMMAND = 0x30,
	SET_COMMAND_RESPONSE = 0x31,

	/* Table A-2: Response Type Defines
	 * RDM Response Type (Slot 16) */
	RESPONSE_TYPE_ACK = 0x00,
	RESPONSE_TYPE_ACK_TIMER = 0x01,
	RESPONSE_TYPE_NACK_REASON = 0x02,  /* See Table A-17 */
	RESPONSE_TYPE_ACK_OVERFLOW = 0x03, /* Additional Response Data available beyond single response length. */
	RESPONSE_TYPE_NOREPLY = 0xff, // This is used internally to indicate, that no response shall be send

	/* RDM Parameter ID's (Slot 21-22)
	 *-- Category - Network Management */
	DISC_UNIQUE_BRANCH = 0x0001,
	DISC_MUTE = 0x0002,
	DISC_UN_MUTE = 0x0003,
	PROXIED_DEVICES = 0x0010,
	PROXIED_DEVICE_COUNT = 0x0011,
	COMMS_STATUS = 0x0015,
	
	/*-- Category - Status Collection */
	QUEUED_MESSAGE = 0x0020, /* See Table A-4 */
	STATUS_MESSAGES = 0x0030, /* See Table A-4 */
	STATUS_ID_DESCRIPTION = 0x0031,
	CLEAR_STATUS_ID = 0x0032,
	SUB_DEVICE_STATUS_REPORT_THRESHOLD = 0x0033, /* See Table A-4 */
	
	DMX_POS_STARTCODE = 0,
	RDM_POS_SUBSTARTCODE = 1,
	RDM_POS_LENGTH = 2,
	RDM_POS_DEST_UID = 3,
	RDM_POS_SRC_UID = 9,

	RDM_POS_PORT_ID = 16,
	RDM_POS_RESPONSE_TYPE = 16,
	
	RDM_POS_COMMAND_CLASS = 20,
	RDM_POS_PID = 21, /* 21-22 */
	RDM_POS_PDL = 23,
	RDM_DISCOVER_REPLY_MAX_FRAMESIZE = 24
};

//#ifdef kernel
// #include <linux/byteorder/generic.h>
//#else
#include <arpa/inet.h>
//#endif
static inline u16 rdm_to_host_short(const u16 x) { return ntohs(x); }
static inline u16 host_to_rdm_short(const u16 x) { return htons(x); }

static inline u16 rdm_to_host_long(const u16 x) { return ntohl(x); }
static inline u16 host_to_rdm_long(const u16 x) { return htonl(x); }


static struct dmx512_framequeue_entry * update_rdm_checksum(struct dmx512_framequeue_entry * frame)
{
	if (frame) {
		uint16_t rdmcs = 0;
		const int rdm_framesize = frame->frame.payload_size - 1;
		if (rdm_framesize > 254)
			return frame;
		frame->frame.data[RDM_POS_RESPONSE_TYPE] = RESPONSE_TYPE_NOREPLY;
		frame->frame.data[RDM_POS_COMMAND_CLASS] |= 1;
		rdmcs = dmx512frame_calculate_rdm_checksum(frame->frame.data,
							   rdm_framesize);
		frame->frame.data[rdm_framesize] = (uint8_t)(rdmcs>>8);
		frame->frame.data[rdm_framesize+1] = (uint8_t)rdmcs;
	}
	return frame;
}


static void dmx512rtuart_stop_rdm_timer(struct dmx512_uart_port * port)
{
	//-- posix timer function
	if (port->rdm_timer.active) {
		port->rdm_timer.active = 0;
		timer_delete(port->rdm_timer.id);
	}
}

static void dmx512rtuart_handle_rdm_timeout(struct dmx512_uart_port * port);
static void __posix_timer_thread_handler(sigval_t arg)
{
	struct dmx512_uart_port * port = (struct dmx512_uart_port *)arg.sival_ptr;
	port->rdm_timer.callTimeoutHandler = 1;
	//printf ("#### posix timeout handler called\n");
}

static void dmx512rtuart_start_rdm_timer(struct dmx512_uart_port * port,
					 const unsigned long timeout_microseconds)
{
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
			printf ("failed to create timer\n");
			return;
		}
		port->rdm_timer.active = 1;
	}

	const long long nanosecs = timeout_microseconds * 1000;
	port->rdm_timer.ts.it_value.tv_sec  = nanosecs / 1000000000;
	port->rdm_timer.ts.it_value.tv_nsec = nanosecs % 1000000000;
	port->rdm_timer.ts.it_interval.tv_sec  = 0;
	port->rdm_timer.ts.it_interval.tv_nsec = 0;

	// TODO maybe we'll need to have an array of itimerspec
	status = timer_settime(port->rdm_timer.id, 0, &port->rdm_timer.ts, 0);
	if (status == -1)
	{
		printf ("failed to adjust timer\n");
	}
}

static void update_rdm_sizes_from_pdl(struct dmx512_framequeue_entry * f)
{
	if (!f)
		return;
	const int frameSizeWithoutChecksum = RDM_DISCOVER_REPLY_MAX_FRAMESIZE + f->frame.data[RDM_POS_PDL];
	// TODO: may add check for frameSizeWithoutChecksum < 255
	f->frame.data[RDM_POS_LENGTH] = frameSizeWithoutChecksum;
	f->frame.payload_size = (frameSizeWithoutChecksum + 2) - 1;
}

static struct dmx512_framequeue_entry * rdm_discovery_reply_collision(struct dmx512_framequeue_entry * f)
{
	if (f)
	{
		// printf ("rdm_discovery_reply_collision   payload_size:%d\n", f->frame.payload_size);
		memcpy (&f->frame.data[RDM_POS_DEST_UID],
			&f->frame.data[RDM_POS_SRC_UID],
			6);
		memset(&f->frame.data[RDM_POS_SRC_UID], 0xFF, 6);
		f->frame.data[RDM_POS_COMMAND_CLASS] = DISCOVERY_COMMAND_RESPONSE;
		f->frame.data[RDM_POS_PDL] = 0;
		update_rdm_sizes_from_pdl(f);
		update_rdm_checksum(f);
	}
	return f;
}

static struct dmx512_framequeue_entry * rdm_discovery_reply_timeout(struct dmx512_framequeue_entry * f)
{
	if (f)
	{
		// printf ("rdm_discovery_reply_timeout   payload_size:%d\n", f->frame.payload_size);
		memcpy (&f->frame.data[RDM_POS_DEST_UID],
			&f->frame.data[RDM_POS_SRC_UID],
			6);
		memset(&f->frame.data[RDM_POS_SRC_UID], 0, 6);
		f->frame.data[RDM_POS_COMMAND_CLASS] = DISCOVERY_COMMAND_RESPONSE;
		f->frame.data[RDM_POS_PDL] = 0;
		update_rdm_sizes_from_pdl(f);
		update_rdm_checksum(f);
	}
	return f;
}

static int dmx512frame_copy(struct dmx512frame * dest, struct dmx512frame * src)
{
	if (dest && src)
		memcpy (dest, src, sizeof(*src));
}

/* @dmx512frame_is_rdm_discover expects that is_rdm has allready been called */
static int dmx512frame_is_rdm_discover(u8 * data, const int count)
{
  return  (data[DMX_POS_STARTCODE] == SC_RDM) &&
		(data[RDM_POS_SUBSTARTCODE] == SC_SUB_MESSAGE) &&
		(data[RDM_POS_COMMAND_CLASS] == DISCOVERY_COMMAND) &&
		(data[RDM_POS_PID]   == (u8)(DISC_UNIQUE_BRANCH>>8)) &&
		(data[RDM_POS_PID+1] == (u8)(DISC_UNIQUE_BRANCH)) &&
		(data[RDM_POS_PDL] == 12);
}


static int dmx512frame_is_rdm_discover_reply(u8 * data, const int count)
{
  return  (data[DMX_POS_STARTCODE] == SC_RDM) &&
		(data[RDM_POS_SUBSTARTCODE] == SC_SUB_MESSAGE) &&
		(data[RDM_POS_COMMAND_CLASS] == DISCOVERY_COMMAND_RESPONSE) &&
		(data[RDM_POS_PID]   == (u8)(DISC_UNIQUE_BRANCH>>8)) &&
		(data[RDM_POS_PID+1] == (u8)(DISC_UNIQUE_BRANCH)) &&
		(data[RDM_POS_PDL] == 0);
}

static int dmx512frame_is_rdm(u8 * data, const int count)
{
	/*
	printf ("dmx512frame_is_rdm SC:%02X  SSC:%02X\n",
		data[DMX_POS_STARTCODE],
		data[RDM_POS_SUBSTARTCODE]);
	*/
	return  (data[DMX_POS_STARTCODE] == SC_RDM) &&
		(data[RDM_POS_SUBSTARTCODE] == SC_SUB_MESSAGE);
}

static u16 dmx512frame_calculate_rdm_checksum(const u8 * data, const int size)
{
	u16 sum = 0;
	int i;
	for (i = 0; i < size; ++i)
		sum += data[i];
	return sum;
}

static int dmx512rtuart_find_preamble(const u8 * data, const int size)
{
	int i;
	for (i = 0; i < size; ++i)
	{
		if (data[i] == 0xAA) // preamble separator byte
			return i;
		else if (data[i] != 0xFE) // response preamble
			return -2;
	}
	return -1; // not yet found preamble separator byte
}

static int dmx512rtuart_complete_rdm_discovery_reply(struct dmx512_framequeue_entry * f)
{
	if (!f)
		return -1;

	const int  count = f->frame.payload_size + 1;
	const u8 * data = &f->frame.data[256];
	int i;

	const int ppos = dmx512rtuart_find_preamble(data, count);
	if (ppos > -1)
	{
		data += ppos + 1;
		for (i = 0; i < 16; i += 2)
		{
			if ((data[i] & 0xAA) != 0xAA)
				return -1;
			if ((data[i+1] & 0x55) != 0x55)
				return -1;
		}
	}
	const u16 calccrc = dmx512frame_calculate_rdm_checksum(data, 12);
	u16 crc = ((u16)(data[12] & data[13])<<8) + (data[14] & data[15]);
#ifdef DMX512_DEBUG_RX_DATA
	printf ("crc:%04X, %04X\n", calccrc, crc);
#endif
	if (calccrc != crc)
		return -1;

	memcpy (&f->frame.data[RDM_POS_DEST_UID],
		&f->frame.data[RDM_POS_SRC_UID],
		6);

	for (i = 0; i < 6; ++i)
		f->frame.data[RDM_POS_SRC_UID + i] = data[2*i] & data[2*i+1];
	f->frame.data[RDM_POS_COMMAND_CLASS] = DISCOVERY_COMMAND_RESPONSE;
	f->frame.data[RDM_POS_PDL] = 0;
	update_rdm_sizes_from_pdl(f);

	// payload_size is the number of dmx-channels in a dmx frame.
	// add one to get the number of bytes and subtract 2 for the size without checksum.
	const int rdm_framesize = f->frame.payload_size - 1;
	const uint16_t rdmcs = dmx512frame_calculate_rdm_checksum(f->frame.data, rdm_framesize);
	f->frame.data[rdm_framesize] = (uint8_t)(rdmcs>>8);
	f->frame.data[rdm_framesize+1] = (uint8_t)rdmcs;

	return 0;
}

static struct dmx512_framequeue_entry* dmx512rtuart_checkout_tx_frame(struct dmx512_uart_port * port)
{
	struct dmx512_framequeue_entry * f = port->tx.frame;
	if (f) {
		port->tx.ptr = 0;
		port->tx.remain = 0;
		port->tx.frame = 0;
	}
	return f;
}

static struct dmx512_framequeue_entry* dmx512rtuart_checkout_rx_frame(struct dmx512_uart_port * port)
{
	struct dmx512_framequeue_entry * f = port->rx.frame;
	if (f) {
		f->frame.payload_size = port->rx.count - 1;
		if (f->frame.startcode == 0x00)
			dmx512_uart_update_automatic_framecount_locking(port);
	}
	if (f) {
		port->rx.data = 0;
		port->rx.count = 0;
		port->rx.frame = 0;
	}
	return f;
}

static void dmx512_uart_update_automatic_framecount_locking(struct dmx512_uart_port * port)
{
	struct dmx512_framequeue_entry * f = port->rx.frame;
	if (f && f->frame.startcode == 0)
	{
		/* automatic locking to a payload size that is received periodicaly on startcode 0 */
		if (port->rx.slot_count_lock.locked)
		{
			if (f->frame.payload_size != port->rx.slot_count_lock.matching_slot_count)
			{
				port->rx.slot_count_lock.locked = 0;
				port->rx.slot_count_lock.matching_frame_count = 0;
				/* start with what we have */
				port->rx.slot_count_lock.matching_slot_count = f->frame.payload_size;
			}
		}
		else
		{
			if (f->frame.payload_size == port->rx.slot_count_lock.matching_slot_count)
			{
				++port->rx.slot_count_lock.matching_frame_count;
				if (port->rx.slot_count_lock.required_count == port->rx.slot_count_lock.matching_frame_count)
					port->rx.slot_count_lock.locked = 1;
			}
			else
			{
				port->rx.slot_count_lock.matching_frame_count = 0;
				port->rx.slot_count_lock.matching_slot_count = f->frame.payload_size;
			}
		}
	}
}

static int dmx512rtuart_transmitter_has_space(struct rtuart *uart, const int space_available)
{
	struct dmx512_uart_port * port = rtuart_to_dmx512_uart_port(uart);
	if (!port->tx.frame)
		return 0;

	if (!port->tx.ptr) { // TODO: this is a fear function.
		dmx512_put_frame(&port->dmx, dmx512rtuart_checkout_tx_frame(port));
		return 0;
	}
	printf ("dmx512rtuart_transmitter_has_space:\n  state=%s\n  remain:%d \n",
		statename(port->state),
		port->tx.remain
		);

	if ((port->state == PORT_STATE_WAIT_FOR_TX_COMPLETE)/* || (port->state == PORT_STATE_IDLE)*/)
	  return 0;

	if (port->state != PORT_STATE_TRANSMIT_DATA) {
		printf("transmitter_has_space with tx-frame and\n   no PORT_STATE_TRANSMIT_DATA 0x%03X\n",
		       port->state);
		printf ("   tx.remain:%lu", port->tx.remain);
		printf ("   written:%ld", port->tx.ptr - port->tx.frame->frame.data );
	}

	if (port->tx.remain > 0)
	{
		const int remain = ((space_available==-1) || (space_available > port->tx.remain))
			? port->tx.remain
			: space_available;

		const int c = (remain > 12) ? 12 : remain; // try with: const int c = remain;

		{
			unsigned int txlevel;
			while (!rtuart_get_tx_fifo_free(uart, &txlevel) && (txlevel != 16));
		}
		const int r = rtuart_write_chars (uart, port->tx.ptr, c);
		port->tx.ptr += c;
		port->tx.remain -= c;
		uart_enable_notification(uart, UART_NOTIFY_TXREADY);
	}
	else
	{
		next_state(port, PORT_STATE_WAIT_FOR_TX_COMPLETE);
		uart_enable_notification(uart, UART_NOTIFY_TXEMPTY);
	}
	return 0;
}


static void dmx512rtuart_handle_rdm_timeout(struct dmx512_uart_port * port)
{
	/*
	 * We only handle timeouts for replies for which we have a request.
	 */
	if (!port->tx.frame)
		return;

	switch(port->state)
	{
	case PORT_STATE_RDMDISC_RECV_PREAMBLE:
	case PORT_STATE_RDMDISC_RECV_DATA:
		//printf ("dmx512rtuart_handle_rdm_timeout(%p)\n %s\n", port, statename(port->state));

		dmx512rtuart_stop_rdm_timer(port);

		if (port->rx.count > 0) {
			dmx512_put_frame(&port->dmx, dmx512rtuart_checkout_rx_frame(port));
			dmx512_received_frame(&port->dmx,
					      rdm_discovery_reply_collision(
						      dmx512rtuart_checkout_tx_frame(port)));
		}
		else {
			dmx512_put_frame(&port->dmx, dmx512rtuart_checkout_rx_frame(port));
			dmx512_received_frame(&port->dmx,
					      rdm_discovery_reply_timeout(
						      dmx512rtuart_checkout_tx_frame(port)));
		}
		break;

	case PORT_STATE_EXPECT_RDM_FRAME:
		/* no answer */
		//printf ("dmx512rtuart_handle_rdm_timeout(%p)\n  PORT_STATE_EXPECT_RDM_FRAME %p\n", port, port->tx.frame);
		dmx512_put_frame(&port->dmx, dmx512rtuart_checkout_rx_frame(port));
		// TODO: fill in reply data that shows a timeout
		dmx512_received_frame(&port->dmx, update_rdm_checksum(dmx512rtuart_checkout_tx_frame(port)));
		//printf ("reply rdm timeout frame\n");
		break;

	case PORT_STATE_RECEIVE_RDMDATA:
		/* we have at least a break, so data is damaged */
		//printf ("dmx512rtuart_handle_rdm_timeout(%p)\n  PORT_STATE_EXPECT_RDM_FRAME %p\n", port, port->tx.frame);
		dmx512_put_frame(&port->dmx, dmx512rtuart_checkout_rx_frame(port));
		// TODO: fill in reply data that shows a damaged reply (like crc error).
		dmx512_received_frame(&port->dmx, update_rdm_checksum(dmx512rtuart_checkout_tx_frame(port)));
		break;
		
	default:
		printf ("dmx512rtuart_handle_rdm_timeout(%p)\n  unhandled state:%d\n",
			port,
			port->state);
		// TODO: should not happen, write a note and delete timer.
		break;
	}
}

static int dmx512rtuart_transmitter_empty(struct rtuart *uart)
{
	struct dmx512_uart_port * port = rtuart_to_dmx512_uart_port(uart);

	printf ("dmx512rtuart_transmitter_empty: state=%s  remain:%d \n",
		statename(port->state),
		port->tx.remain
		);

	if (port->state != PORT_STATE_WAIT_FOR_TX_COMPLETE)
	  {
		printf("transmitter_empty and not in PORT_STATE_WAIT_FOR_TX_COMPLETE\n");
		return 0;
	  }

	if (port->enable_rs485_transmitter)
		port->enable_rs485_transmitter(port, 0); // input
	uart_enable_notification(port->uart, UART_NOTIFY_LINEEVENT);
	/* flush the fifo when we switch back to receive data, as there may be
	 * garbage or data we transmitted in the receive fifo - half duplex
	 */
	//printf ("flush receiver fifo as tx is complete\n");
	rtuart_flush_fifo (uart, 0, 1); // uart_flush_rxfifo(uart);

	//TODO:check if notify makes sense
	uart_enable_notification(port->uart, UART_NOTIFY_LINEEVENT);

	if (port->tx.frame && (port->tx.frame->frame.flags & DMX512_FLAG_IS_RDM))
	{
		//printf ("RDM");
		if (port->tx.frame->frame.flags & DMX512_FLAG_IS_RDM_DISC)
		{
			//printf (" Discovery\n");
			port->rx.frame = dmx512_get_frame(&port->dmx);
			if (port->rx.frame)
			{
				dmx512frame_copy(&port->rx.frame->frame, &port->tx.frame->frame);
				next_state(port, PORT_STATE_RDMDISC_RECV_PREAMBLE);
				port->rx.expected_slots_count = 0;
				port->rx.data = &port->rx.frame->frame.data[256];
				port->rx.count = 0;
				//printlog("start rdm-disc reply\n");
				dmx512rtuart_start_rdm_timer(port, DISCOVERY_REQUEST_REPLY_TIMEOUT);
				uart_enable_notification(uart, UART_NOTIFY_RXAVAILABLE);
				return 0;
			}
			// else printf (" (no rx-frame)");
		}
		else
		{
			//printf (" Frame\n");
			/* an RDM frame can not be more than 256 bytes in size.
			 * The size field will be updated once the real size is known.
			 */
			port->rx.expected_slots_count = 256;
			//next_state(port, PORT_STATE_RECEIVE_RDMDATA);
			next_state(port, PORT_STATE_EXPECT_RDM_FRAME);
			dmx512rtuart_start_rdm_timer(port, TIMEOUT_RDM_EOP_TO_SOP);
			//printlog("start rdm-frame reply\n");
			return 0;
		}
		//printf ("RDM\n");
	}
	dmx512_put_frame(&port->dmx, dmx512rtuart_checkout_tx_frame(port));
	next_state(port, PORT_STATE_IDLE);
	return 0;
}

static int is_between(const int pos,
		      const int start,
		      const int end)
{
	return (start <= pos) && (pos <= end);
}

static void dmx512_check_received_data(struct dmx512_uart_port * port,
				       int  start_pos, /* 0 = startcode */
				       int  size,
				       u8 * data)
{
	if (port->rx.frame->frame.flags & DMX512_FLAG_IS_RDM_DISC)
		return;
	if (port->rx.frame == 0)
		return;

	//-- if we have received enough data to know the size of an RDM frame:
	//--
	/* this data contains the startcode */
	if (!(port->rx.frame->frame.flags & DMX512_FLAG_IS_RDM))
	{
		if (/* is_between(0, start_pos, start_pos+size) */
		    (start_pos == 0) && (size > 0))
		{
			//printf ("RX: check startcode (0x%02X)\n", data[0]);
			/* this is an RDM frame */
			if (data[0] == SC_RDM)
				port->rx.frame->frame.flags |= DMX512_FLAG_IS_RDM;
			else if (data[0] == 0)
			{
				if (port->rx.slot_count_lock.locked)
					port->rx.expected_slots_count = port->rx.slot_count_lock.matching_slot_count;
				else
					port->rx.expected_slots_count = 512;
			}
		}
	}

	if (port->rx.frame->frame.flags & DMX512_FLAG_IS_RDM)
	{
		if (is_between(RDM_POS_SUBSTARTCODE, start_pos, start_pos+size))
		{
			if(data[RDM_POS_SUBSTARTCODE-start_pos] != SC_SUB_MESSAGE)
				port->rx.frame->frame.flags &= ~DMX512_FLAG_IS_RDM;
		}
	}

	if (port->rx.frame->frame.flags & DMX512_FLAG_IS_RDM)
	{
		if (is_between(RDM_POS_LENGTH, start_pos, start_pos+size))
		{
			/* we include checksum and exclude startcode. */
			port->rx.expected_slots_count = data[RDM_POS_LENGTH-start_pos] + 2 - 1;

			dmx512rtuart_stop_rdm_timer(port);
			/* adjust timer value to real size of frame. */
			const int timePerSlotMicroseconds = TIMEOUT_RDMFRAME_PEROCTET;
			const int overheadMicroseconds = TIMEOUT_RDMFRAME_STATIC;
			dmx512rtuart_start_rdm_timer(port, overheadMicroseconds + (port->rx.expected_slots_count - RDM_POS_LENGTH) * timePerSlotMicroseconds);
		}
	}

	if (port->rx.frame->frame.flags & DMX512_FLAG_IS_RDM)
	{
		/* we have not yet checked for RDM discovery */
		if (is_between(RDM_POS_PDL, start_pos, start_pos+size))
		{
			if (dmx512frame_is_rdm_discover(port->rx.frame->frame.data, port->rx.count + size))
				port->rx.frame->frame.flags |= DMX512_FLAG_IS_RDM_DISC;
		}
	}
}



static int dmx512rtuart_receiver_data_available(struct rtuart * uart, const int _count)
{
	const int expected_payload_size = 512;
	struct dmx512_uart_port * port = rtuart_to_dmx512_uart_port(uart);
	const int count = (_count==-1) ? 1 : _count;

	const int max_size = port->rx.expected_slots_count + 1;
	const int toread = (port->rx.count + count < max_size)
		? count : (max_size - port->rx.count);

// TODO: find error why receiver is enabled while in TX-State
	if ((port->state >= PORT_STATE_FIRST_TXSTATE) &&
	    (port->state < PORT_STATE_LAST_TXSTATE))
	{
		// printf ("## dmx512rtuart_receiver_data_available called in state %s\n", statename(port->state));
		return 0;
	}


	if (port->rx.frame == 0)
	{
		rtuart_flush_fifo (uart, 0, 1);
		next_state_label(port, PORT_STATE_IDLE, "1");
		return 0;
	}

	switch (port->state)
	{
	case PORT_STATE_EXPECT_STARTCODE:
	case PORT_STATE_RECEIVE_DMXDATA:
		next_state(port, PORT_STATE_RECEIVE_DMXDATA);
	case PORT_STATE_RECEIVE_RDMDATA:
		{
			if (port->rx.data == 0)
			{
				printf ("FATAL: NO DATA BUFFER  frame:%p  %s\n", port->rx.frame, statename(port->state));
				exit(1);
			}

			const int ret = rtuart_read_chars(uart, port->rx.data, toread);
			if (ret > 0)
			{
				dmx512_check_received_data(port,
							   port->rx.count,
							   ret,
							   port->rx.data);
				port->rx.data += ret;
				port->rx.count += ret;

				if ((port->rx.count >= max_size) && port->rx.frame)
				{
					printf ("flush old frame(rx.count>=max_size{%d}): %02X %02X %02X\n",
						max_size,
						port->rx.frame->frame.data[0],
						port->rx.frame->frame.data[1],
						port->rx.frame->frame.data[2]);

					// printf ("rx-frame-completed tx:%p count:%lu\n", port->tx.frame, port->rx.count);

					if (port->tx.frame && (port->tx.frame->frame.flags & DMX512_FLAG_IS_RDM))
						dmx512rtuart_stop_rdm_timer(port);

					dmx512_put_frame(&port->dmx, dmx512rtuart_checkout_tx_frame(port));
					dmx512_received_frame(&port->dmx, dmx512rtuart_checkout_rx_frame(port));
					next_state_label(port, PORT_STATE_IDLE, "2");
				}
				else
					uart_enable_notification(uart, UART_NOTIFY_RXAVAILABLE);
			}
			if (count > toread)
				rtuart_flush_fifo (uart, 0, 1); // uart_flush_rxfifo(uart);
		}
		break;

	case PORT_STATE_RDMDISC_RECV_PREAMBLE:
		//printlog("SOP rdm-disc reply\n");
		dmx512rtuart_stop_rdm_timer(port);

		// we expect all rdm discovery reply data to be received then
		dmx512rtuart_start_rdm_timer(port, TIMEOUT_RDMDISCOVERY_LENGTH);

	case PORT_STATE_RDMDISC_RECV_DATA:
	        {
			const int ret = rtuart_read_chars(uart, port->rx.data, count);
			if (ret > 0)
			{
#ifdef DMX512_DEBUG_RX_DATA
				{
					int i;
					printf ("rx:[%d,%d]:", _count, count);
					for (i = 0; i < count; ++i)
						printf (" %02X", port->rx.data[i]);
					printf (" expect:%d cnt:%d\n", port->rx.expected_slots_count, port->rx.count);
				}
#endif
				if (port->state == PORT_STATE_RDMDISC_RECV_PREAMBLE)
				{
					//--- Check received data for RDM Discovery Data
					const int p = dmx512rtuart_find_preamble(port->rx.data, ret);
					if (p == -2) // collision
					{
						// printf ("Collision(preamble)\n");
						dmx512rtuart_stop_rdm_timer(port);
						dmx512_put_frame(&port->dmx, dmx512rtuart_checkout_rx_frame(port));

						dmx512_received_frame(&port->dmx,
								      rdm_discovery_reply_collision(
									      dmx512rtuart_checkout_tx_frame(port)));
						next_state_label(port, PORT_STATE_IDLE, "3");
					}
					else if (p > -1) // found preamble
					{
						port->rx.expected_slots_count = port->rx.count + p + 16;
						next_state(port, PORT_STATE_RDMDISC_RECV_DATA);
					}
					//--- end of RDM fixup
				}

				port->rx.count += ret;
				port->rx.data += ret;

				/*
				 * It might happen, that the buffer read is so large,
				 * that the preamble and the full buffer fits in.
				 * This might be the case if the RXFIFO is 32 bytes in size or bigger.
				 */
				if (port->state == PORT_STATE_RDMDISC_RECV_DATA)
				{
					const int max_size = port->rx.expected_slots_count + 1;
					if ((port->rx.count >= max_size) && port->rx.frame)
					{
						// printf ("DISCOVERY COMPLETE\n");
						dmx512rtuart_stop_rdm_timer(port);

						struct dmx512_framequeue_entry * rxframe = dmx512rtuart_checkout_rx_frame(
port);
						if (dmx512rtuart_complete_rdm_discovery_reply(rxframe) < 0)
						{
							dmx512_received_frame(&port->dmx,
									      rdm_discovery_reply_collision(dmx512rtuart_checkout_tx_frame(port)));
							dmx512_put_frame(&port->dmx, rxframe);
						}
						else
						{
							dmx512_put_frame(&port->dmx, dmx512rtuart_checkout_tx_frame(port));
							dmx512_received_frame(&port->dmx, rxframe);
						}
						next_state_label(port, PORT_STATE_IDLE, "4");
					}
					else
						uart_enable_notification(uart, UART_NOTIFY_RXAVAILABLE);
				}
				else
					uart_enable_notification(uart, UART_NOTIFY_RXAVAILABLE);
			}
		}
		break;

	default:
		rtuart_flush_fifo (uart, 0, 1); // uart_flush_rxfifo(uart);
		break;
	}
	return 0;
}


static int dmx512rtuart_modem_input_changed(struct rtuart * uart, const unsigned long values, const unsigned long change)
{
	(void)uart;
	//printk("input changed change:%02lX value:%02lX\n", change, values);
}

static int dmx512rtuart_line_status_event(struct rtuart * uart, const unsigned long eventmask)
{
	struct dmx512_uart_port * port = rtuart_to_dmx512_uart_port(uart);

	if ((port->state >= PORT_STATE_FIRST_TXSTATE) &&
	    (port->state < PORT_STATE_LAST_TXSTATE))
	{
		// printf ("## dmx512rtuart_line_status_event called in state %s\n", statename(port->state));
		// uart_enable_notification(uart, UART_NOTIFY_LINEEVENT);
		return 0;
	}

	if (eventmask & RTUART_EVENT_BREAK)
	{
		printf ("--------- Received Break ------------\n");
		// TODO: check if this makes sense here.
		if ((port->state <= PORT_STATE_RECEIVE_DMXDATA) &&
		    port->rx.frame)
		{
			if (port->state != PORT_STATE_EXPECT_STARTCODE) {
				printf ("flush old frame(break): %02X %02X %02X\n",
					port->rx.frame->frame.data[0],
					port->rx.frame->frame.data[1],
					port->rx.frame->frame.data[2]);
				dmx512_received_frame(&port->dmx, dmx512rtuart_checkout_rx_frame(port));
				port->rx.frame = 0;
				if ((port->state >= PORT_STATE_FIRST_RXSTATE)
				    && (port->state <= PORT_STATE_LAST_RXSTATE))
					next_state(port, PORT_STATE_IDLE);
			}
		}

		switch (port->state)
		{
		case PORT_STATE_IDLE: // start a new reception
		case PORT_STATE_EXPECT_RDM_FRAME:
			if (port->state == PORT_STATE_EXPECT_RDM_FRAME)
			{
				//printlog("SOP rdm-frame reply\n");
				dmx512rtuart_stop_rdm_timer(port);
				/* restart the timer, as the frame can time out. */
				dmx512rtuart_start_rdm_timer(port, TIMEOUT_RDM_SOP_TO_EOP);
			}

			port->rx.frame = dmx512_get_frame(&port->dmx);
			if (port->rx.frame)
			{
				port->rx.data = port->rx.frame->frame.data;
				port->rx.count = 0;
				memset(port->rx.frame->frame.data, 0xCE, sizeof(port->rx.frame->frame.data));

				if (port->state == PORT_STATE_EXPECT_RDM_FRAME)
				{
					// printf ("received RDM-Reply break\n");
					port->rx.expected_slots_count = 256;
					next_state(port, PORT_STATE_RECEIVE_RDMDATA);
				}
				else
				{
					next_state(port, PORT_STATE_EXPECT_STARTCODE);
					 /* change this once we know the startcode */
					port->rx.expected_slots_count = 512;
				}
				uart_enable_notification(uart, UART_NOTIFY_RXAVAILABLE);
			}
			break;

			/* we somehow received two concecutive breaks.
			 * Just reuse the frame we have.
			 */
		case PORT_STATE_EXPECT_STARTCODE:
			if (port->rx.frame) {
				printf ("--> reuse frame.");
				port->rx.data = port->rx.frame->frame.data;
				port->rx.count = 0;
			}
			else 

		case PORT_STATE_RDMDISC_RECV_PREAMBLE:
		case PORT_STATE_RDMDISC_RECV_DATA:
			//TODO: handle this a a collision
			printf ("break: PORT_STATE_RDMDISC_RECV_* => collision\n");
			break;

		case PORT_STATE_TRANSMIT_BREAK:
			/* this is the break we transmitted. */
			break;
		default:
			printf ("Unhandled state for BREAK:%d\n", port->state);
			break;
		}
	}
	uart_enable_notification(uart, UART_NOTIFY_LINEEVENT);
}

static const struct rtuart_client_callbacks dmx512rtuart_callbacks =
{
  .transmitter_has_space = dmx512rtuart_transmitter_has_space,
  .transmitter_empty = dmx512rtuart_transmitter_empty,
  .receiver_data_available = dmx512rtuart_receiver_data_available,
  .modem_input_changed = dmx512rtuart_modem_input_changed,
  .line_status_event = dmx512rtuart_line_status_event
};



static int dmx512_rtuart_client_send_frame (struct dmx512_port * dmxport, struct dmx512_framequeue_entry * frame)
{
	// printf ("dmx512_rtuart_client_send_frame\n");

	//struct dmx512_uart_port * rtuart_to_dmx512_uart_port(struct rtuart *uart);

	struct dmx512_uart_port * port = dmx512port_to_dmx512_uart_port(dmxport);

	/* if we plan to transmit data, we need to kill a reception
	 * maybe we can check how old the last dmx-slot of the frame is.
	 * If it is very young (<132us) or we have an rx-lock, then we
	 * should wait for the end of the receiption.
	 */
	if (port->rx.frame)
	{
		printf ("flush old frame(send_frame): %02X %02X %02X\n",
			port->rx.frame->frame.data[0],
			port->rx.frame->frame.data[1],
			port->rx.frame->frame.data[2]);

		dmx512_received_frame(&port->dmx, dmx512rtuart_checkout_rx_frame(port));
		port->rx.frame = 0;
		if ((port->state >= PORT_STATE_FIRST_RXSTATE)
		    && (port->state <= PORT_STATE_LAST_RXSTATE))
			next_state(port, PORT_STATE_IDLE);
	}

	if (port->tx.frame)
	{
		dmx512_put_frame(dmxport, frame);
	}
	else
	{
		next_state(port, PORT_STATE_TRANSMIT_BREAK);
		struct rtuart * uart = port->uart;
		port->tx.frame = frame;
		port->tx.ptr   = frame->frame.data;
		port->tx.remain = 1 + frame->frame.payload_size;

		// dump_dmx512_frame(port->tx.frame, "send ");
		if (dmx512frame_is_rdm(port->tx.ptr, port->tx.remain))
		{
			// printf ("TX RDM %d", dmx512frame_is_rdm_discover(port->tx.ptr, port->tx.remain));
			port->tx.frame->frame.flags |= DMX512_FLAG_IS_RDM;
			if (dmx512frame_is_rdm_discover(port->tx.ptr, port->tx.remain))
			{
				port->tx.frame->frame.flags |= DMX512_FLAG_IS_RDM_DISC;
				// printf (" Discovery");
			}
#if 1
			else if (dmx512frame_is_rdm_discover_reply(port->tx.ptr, port->tx.remain)) {
				u16 checksum = 0;
				printf ("!! sending discovery reply(1): %lu, %p, %d\n",
					(size_t)(port->tx.ptr-port->tx.frame->frame.data),
					port->tx.ptr,
					port->tx.remain
					);
				port->tx.ptr[256] = 0xFE; // 1
				port->tx.ptr[257] = 0xFE; // 2
				port->tx.ptr[258] = 0xFE; // 3
				port->tx.ptr[259] = 0xFE; // 4
				port->tx.ptr[260] = 0xFE; // 5
				port->tx.ptr[261] = 0xFE; // 6
				port->tx.ptr[262] = 0xFE; // 7
				port->tx.ptr[263] = 0xAA; // 8
				port->tx.ptr[264] = 0xAA | port->tx.ptr[RDM_POS_SRC_UID]; // 9
				port->tx.ptr[265] = 0x55 | port->tx.ptr[RDM_POS_SRC_UID]; // 10
				port->tx.ptr[266] = 0xAA | port->tx.ptr[RDM_POS_SRC_UID+1]; // 11
				port->tx.ptr[267] = 0x55 | port->tx.ptr[RDM_POS_SRC_UID+1]; // 12
				port->tx.ptr[268] = 0xAA | port->tx.ptr[RDM_POS_SRC_UID+2]; // 13
				port->tx.ptr[269] = 0x55 | port->tx.ptr[RDM_POS_SRC_UID+2]; // 14
				port->tx.ptr[270] = 0xAA | port->tx.ptr[RDM_POS_SRC_UID+3]; // 15
				port->tx.ptr[271] = 0x55 | port->tx.ptr[RDM_POS_SRC_UID+3]; // 16
				port->tx.ptr[272] = 0xAA | port->tx.ptr[RDM_POS_SRC_UID+4]; // 17
				port->tx.ptr[273] = 0x55 | port->tx.ptr[RDM_POS_SRC_UID+4]; // 18
				port->tx.ptr[274] = 0xAA | port->tx.ptr[RDM_POS_SRC_UID+5]; // 19
				port->tx.ptr[275] = 0x55 | port->tx.ptr[RDM_POS_SRC_UID+5]; // 20
				checksum = dmx512frame_calculate_rdm_checksum(&port->tx.ptr[264], 12);
				port->tx.ptr[276] = 0xAA | (u8)(checksum>>8); // 21
				port->tx.ptr[277] = 0x55 | (u8)(checksum>>8); // 22
				port->tx.ptr[278] = 0xAA | (u8)checksum; // 23
				port->tx.ptr[279] = 0x55 | (u8)checksum; // 24
				port->tx.ptr += 256;
				port->tx.remain = 24;
				frame->frame.flags |= DMX512_FLAG_NOBREAK;
				printf ("!! sending discovery reply(2): %lu, %p, %d\n",
					(size_t)(port->tx.ptr-port->tx.frame->frame.data),
					port->tx.ptr,
					port->tx.remain
					);
			}
#endif
			// printf ("\n");
		}

		const int min_break_time_us = 176;
		const unsigned long break_us = (4*frame->frame.breaksize > min_break_time_us) ? 4*frame->frame.breaksize : min_break_time_us;
		uart_disable_notification(uart, UART_NOTIFY_LINEEVENT | UART_NOTIFY_RXAVAILABLE);

		if (port->enable_rs485_transmitter)
			port->enable_rs485_transmitter(port, 1); // output

		// start_timer_nanoseconds(&port->tx.breaktimer, 1000*break_us);
		// try to move this to some timer or bottomhalf.
		if (0 == (frame->frame.flags & DMX512_FLAG_NOBREAK)) {
			rtuart_set_break(uart, 1);
			usleep(break_us);
			rtuart_set_break(uart, 0);
		}
		next_state(port, PORT_STATE_TRANSMIT_DATA);
		uart_enable_notification(uart, UART_NOTIFY_TXREADY);
	}
}

int dmx512_rtuart_client_is_busy(struct rtuart * uart)
{
	struct dmx512_uart_port * port = rtuart_to_dmx512_uart_port(uart);
	return port->tx.frame != 0;
}

static void dmx512_rtuart_start(struct dmx512_uart_port * port)
{
	struct rtuart * uart = port->uart;
	rtuart_set_format(uart, 8, 'n', 20);

	rtuart_set_baudrate(uart, 250000);
	rtuart_set_fifo_enable(uart, 1);
	rtuart_set_rxfifo_trigger_level (uart, 8);
	rtuart_set_txfifo_trigger_level (uart, 2);

	uart_enable_notification(uart ,
				 // UART_NOTIFY_TXEMPTY |
				 //UART_NOTIFY_TXREADY |
				 //UART_NOTIFY_RXAVAILABLE |
				 UART_NOTIFY_LINEEVENT |
				 UART_NOTIFY_INPUTCHANGED);
}

// @dir Changes the direction of the RS485 transceiver 1 = output, 0 = input
void dmx512_rtuart_client_enable_rs485_transmitter(struct dmx512_uart_port * port, const int on)
{
	// printf ("enable_rs485_transmitter: %s\n", on ? "Output" : "Input");
	rtuart_set_modem_lines(port->uart,
			       RTUART_OUTPUT_RTS,
			       on ? 0 : RTUART_OUTPUT_RTS);
}

struct dmx512_port * dmx512_rtuart_client_create(struct rtuart * uart)
{
	struct dmx512_uart_port * port = (struct dmx512_uart_port *)malloc(sizeof(*port));
	if (port == 0)
		return 0;
	memset (port, 0, sizeof(*port));
	uart->client = &port->client;
	port->uart = uart;
	port->client.callbacks = &dmx512rtuart_callbacks;

	next_state(port, PORT_STATE_DOWN);
	port->enable_rs485_transmitter = dmx512_rtuart_client_enable_rs485_transmitter;

	strcpy(port->dmx.name, "uio-dmx");
	port->dmx.capabilities = 0; // DMX512_CAP_RDM
	port->dmx.send_frame = dmx512_rtuart_client_send_frame;

	// dmx512_add_port(dmx_device, &port->dmx);
	
	// may add a callback "start" thaht calls this.
	dmx512_rtuart_start(port);
	if (port->enable_rs485_transmitter)
		port->enable_rs485_transmitter(port, 0); // input
	uart_enable_notification(port->uart, UART_NOTIFY_LINEEVENT);

	next_state(port, PORT_STATE_IDLE);
	return &port->dmx;
}



void dmx512_rtuart_periodical(struct rtuart * uart)
{
	struct dmx512_uart_port * port = rtuart_to_dmx512_uart_port(uart);
	if (port->rdm_timer.callTimeoutHandler) {
		//printf ("dmx512_rtuart_periodical do work\n");
		port->rdm_timer.callTimeoutHandler = 0;
		dmx512rtuart_handle_rdm_timeout(port);
	}
}
