#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FUSE_USE_VERSION 30
#define _FILE_OFFSET_BITS 64
#include <fuse/cuse_lowlevel.h>
#include <fuse/fuse_opt.h>

#include <rtuart.h>
#include <rtuart_ioctl.h>
//#include <rtuart_priv.h>
//#include <rtuart_pc16c550.h>
//#include <uio32_bus.h>

#include <rtuart_factory.h>

#include <kernel.h>

static struct rtuart * g_uart = 0;

struct rtuart_read_event_entry
{
	struct rtuart_read_event_entry * next;
	struct rtuart_read_event e;
};
static struct rtuart_read_event_entry * event_head = 0;
static struct rtuart_read_event_entry * event_tail = 0;

static void add_event(struct rtuart_read_event_entry * e)
{
	printf ("add_event(what:%d)\n", e->e.what);
	e->next = 0;
	if (event_tail)
		event_tail->next = e;
	event_tail = e;
	if (!event_head)
		event_head = e;
}

static struct rtuart_read_event_entry * get_event()
{
	if (event_head) {
		struct rtuart_read_event_entry * e = event_head;
		event_head = event_head->next;
		if (event_head == 0)
			event_tail = 0;
		return e;
	}
	return 0;
}

struct rtuart_read_event_entry * alloc_event()
{
	struct rtuart_read_event_entry * e = malloc(sizeof(*e));
	if (e)
		memset(e, 0, sizeof(*e));
	return e;
}



static void cusetest_open(fuse_req_t req, struct fuse_file_info *fi) {
    printf("open\n");
    fuse_reply_open(req, fi);
}

static void cusetest_read(fuse_req_t req, size_t size, off_t off,
                         struct fuse_file_info *fi) {
    struct rtuart_read_event_entry * e = get_event();
    if (e)
    {
	    fuse_reply_buf(req, (unsigned char *)(&e->e), sizeof(e->e));
	    free(e);
    }
    else
	    fuse_reply_buf(req, 0, 0);
}

static void cusetest_write(fuse_req_t req, const char *buf, size_t size,
                          off_t off, struct fuse_file_info *fi) {
    printf("write (%lu bytes)\n", size);

    struct rtuart_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    buffer.data = (unsigned char *)buf;
    buffer.size = 0;
    buffer.validcount = size;
    buffer.triggermask |= 1 | 2;
    buffer.trigger[0] = 2;
    buffer.trigger[1] = 12;
    rtuart_tx_start  (g_uart, &buffer);

    // wait for tx_end2 to be called.
    // get_semaphore(&txempty_semaphore);

    fuse_reply_write(req, size);
}

static void cusetest_ioctl(fuse_req_t req, int cmd, void *arg,
			   struct fuse_file_info *fi, unsigned flags,
			   const void *in_buf, size_t in_bufsz, size_t out_bufsz) {
	// printf("ioctl %d: insize: %lu outsize: %lu\n", cmd, in_bufsz, out_bufsz);

	switch (cmd) {
	case RTUART_IOCTL_S_FMT:
	case RTUART_IOCTL_S_BAUDRATE:
	case RTUART_IOCTL_S_CONTROL:
	case RTUART_IOCTL_C_CONTROL:
	case RTUART_IOCTL_S_NOTIFY:
	case RTUART_IOCTL_C_NOTIFY:
		if (in_bufsz == 0) {
			struct iovec iov = { arg, sizeof(unsigned long) };
			fuse_reply_ioctl_retry(req, &iov, 1, NULL, 0);
		} else {
			unsigned long value = *((unsigned long*)in_buf);
			switch (cmd) {
			case RTUART_IOCTL_S_FMT:
				rtuart_set_format(g_uart,
						  (unsigned char)(value>>16),
						  (char)(value>>8),
						  (unsigned char)value);
				break;
			case RTUART_IOCTL_S_BAUDRATE:
				rtuart_set_baudrate(g_uart, value);
				break;
			case RTUART_IOCTL_S_CONTROL:
				rtuart_set_control(g_uart, value);
				break;
			case RTUART_IOCTL_C_CONTROL:
				rtuart_clr_control(g_uart, value);
				break;
			case RTUART_IOCTL_S_NOTIFY:
				rtuart_set_notify(g_uart, value);
				break;
			case RTUART_IOCTL_C_NOTIFY:
				rtuart_clr_notify(g_uart, value);
				break;
			}
			fuse_reply_ioctl(req, 0, NULL, 0);
		}
		break;

	case RTUART_IOCTL_G_FMT:
	case RTUART_IOCTL_G_BAUDRATE:
	case RTUART_IOCTL_G_CONTROL:
	case RTUART_IOCTL_G_NOTIFY:
		if (out_bufsz == 0) {
			struct iovec iov = { arg, sizeof(unsigned long) };
			fuse_reply_ioctl_retry(req, NULL, 0, &iov, 1);
		} else {
			unsigned long value = 0;

			switch (cmd)
			{
			case RTUART_IOCTL_G_FMT:
				break;

			case RTUART_IOCTL_G_BAUDRATE:
				rtuart_get_baudrate(g_uart, &value);
				break;

			case RTUART_IOCTL_G_CONTROL:
				rtuart_get_control(g_uart, &value);
				break;

			case RTUART_IOCTL_G_NOTIFY:
				rtuart_get_notify(g_uart, &value);
				break;
			}
			fuse_reply_ioctl(req, 0, &value, sizeof(unsigned long));
		}
		break;

	default:
		printf ("unhandled ioctl\n");
		break;
	}
}

static const struct cuse_lowlevel_ops cusetest_clop = {
        .open           = cusetest_open,
        .read           = cusetest_read,
        .write          = cusetest_write,
        .ioctl          = cusetest_ioctl,
};




static void rtuart_dev_tx_start (struct rtuart * uart,
				 struct rtuart_buffer * buffer)
{
	printf ("rtuart_dev_tx_start()\n");
	struct rtuart_read_event_entry * e = alloc_event();
	if (e)
	{
		e->e.what = RTUART_WHAT_CALLBACK_TX_START;
		add_event(e);
	}
}

static void rtuart_dev_tx_end1 (struct rtuart * uart,
				struct rtuart_buffer * buffer)
{
	printf ("rtuart_dev_tx_end1()\n");
	struct rtuart_read_event_entry * e = alloc_event();
	if (e)
	{
		e->e.what = RTUART_WHAT_CALLBACK_TX_END1;
		add_event(e);
	}
}

static void rtuart_dev_tx_end2 (struct rtuart * uart,
				struct rtuart_buffer * buffer)
{
	printf ("rtuart_dev_tx_end2()\n");
	struct rtuart_read_event_entry * e = alloc_event();
	if (e)
	{
		e->e.what = RTUART_WHAT_CALLBACK_TX_END2;
		add_event(e);
	}
	// put_semaphore(&txempty_semaphore);
}

static void rtuart_dev_input_change (struct rtuart * uart,
				     unsigned long eventmask,
				     unsigned long values)
{
	printf ("rtuart_dev_input_change(...)\n");
	struct rtuart_read_event_entry * e = alloc_event();
	if (e)
	{
		e->e.what = RTUART_WHAT_CALLBACK_INPUT_CHANGE;
		e->e.input_change.eventmask = eventmask;
		e->e.input_change.values = values;
		add_event(e);
	}
}


static void rtuart_dev_rx_err (struct rtuart * uart,
			       unsigned long errmask)
{
	printf ("rtuart_dev_rx_err()\n");
	struct rtuart_read_event_entry * e = alloc_event();
	if (e)
	{
		e->e.what = RTUART_WHAT_CALLBACK_RX_ERR;
		e->e.rx_err.error_mask = errmask;
		add_event(e);
	}
}

static void rtuart_dev_rx_char (struct rtuart * uart,
				const unsigned short c)
{
	printf ("rtuart_dev_rx_char()\n");
	struct rtuart_read_event_entry * e = alloc_event();
	if (e)
	{
		e->e.what = RTUART_WHAT_CALLBACK_RX_CHAR;
		add_event(e);
	}
}

static void rtuart_dev_rx_end (struct rtuart * uart,
			       struct rtuart_buffer * buffer)
{
	printf ("rtuart_dev_rx_end()\n");
	struct rtuart_read_event_entry * e = alloc_event();
	if (e)
	{
		e->e.what = RTUART_WHAT_CALLBACK_RX_END;
		add_event(e);
	}
}

static void rtuart_dev_rx_timeout (struct rtuart * uart,
				   struct rtuart_buffer * buffer)
{
	printf ("rtuart_dev_rx_timeout()\n");
	struct rtuart_read_event_entry * e = alloc_event();
	if (e)
	{
		e->e.what = RTUART_WHAT_CALLBACK_RX_TIMEOUT;
		add_event(e);
	}
}

static void rtuart_dev_rx_trigger(struct rtuart * uart,
				  struct rtuart_buffer * buffer,
				  const unsigned long triggermask
	)
{
	printf ("rtuart_dev_rx_trigger(triggermask:%08lX)\n", triggermask);
	struct rtuart_read_event_entry * e = alloc_event();
	if (e)
	{
		e->e.what = RTUART_WHAT_CALLBACK_RX_TRIGGER;
		e->e.rx_trigger.trigger_mask = triggermask;
		add_event(e);
	}
}


static struct rtuart_callbacks  rtuart_dev_callbacks =
{
	.tx_start   = rtuart_dev_tx_start,
	.tx_end1    = rtuart_dev_tx_end1,
	.tx_end2    = rtuart_dev_tx_end2,
	.input_change = rtuart_dev_input_change,
	.rx_err     = rtuart_dev_rx_err,
	.rx_char    = rtuart_dev_rx_char,
	.rx_end     = rtuart_dev_rx_end,
	.rx_timeout = rtuart_dev_rx_timeout,
	.rx_trigger = rtuart_dev_rx_trigger,
};


//==================================================================


int main(int argc, char** argv) {
	// -f: run in foreground, -d: debug ouput
	// Compile official example and use -h
	const char* cusearg[] = { "test", "-f" /*, "-d"*/ };
	const char* devarg[]  = { "DEVNAME=uart0" };
	struct cuse_info ci;
	memset(&ci, 0x00, sizeof(ci));
	ci.flags = CUSE_UNRESTRICTED_IOCTL;
	ci.dev_info_argc = sizeof(devarg) / sizeof(devarg[0]);
	ci.dev_info_argv = devarg;

	if (argc <= 1) {
		printf ("usage: %s [pc16c550|pl011] <ui0-device>\n", argv[0]);
		return 1;
	}

	g_uart = rtuart_create(argv[1], argv[2]);
	if (g_uart == 0) {
		fprintf(stderr, "failed to create uart\n");
		return 1;
	}

	rtuart_assign_callbacks(g_uart, &rtuart_dev_callbacks, 0);

	
	const int ret = cuse_lowlevel_main(sizeof(cusearg)/sizeof(cusearg[0]), (char**) &cusearg,
				  &ci, &cusetest_clop, NULL);
	// rtuart_destroy(g_uart);

	printf ("Stopping\n");
	rtuart_delete(g_uart);
}
