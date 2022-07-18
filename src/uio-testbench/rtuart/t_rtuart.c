#include <rtuart.h>
#include <rtuart_priv.h>
#include <rtuart_values.h>
#include <rtuart_factory.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <kernel.h>

#include <gpiod.h> // For gpiod_*.

static struct rtuart * uart = 0;
static volatile int dmx_tx_ready = 0;
static struct rtuart_buffer dmx_tx_buffer;

struct {
	struct gpiod_chip * chip;
	struct gpiod_line * line;
} gpio_dmxtxen;

static void rs485txen_set(const int en)
{
	// printf ("rs485txen_set(en:%d)\n", en);
	if (gpio_dmxtxen.line) {
		if (gpiod_line_set_value(gpio_dmxtxen.line, en))
			printk (KERN_WARNING"failed to set direction gpio\n");
	}
	else {
		if (en)
			rtuart_clr_control(uart, RTUART_CONTROL_RTS);
		else
			rtuart_set_control(uart, RTUART_CONTROL_RTS);
	}
}

static int rs485txen_init(const char * kind)
{
	gpio_dmxtxen.chip = 0;
	gpio_dmxtxen.line = 0;
	if (strcmp(kind, "pl011") == 0) {
		gpio_dmxtxen.chip = gpiod_chip_open_by_name("gpiochip0");
		if (gpio_dmxtxen.chip) {
			gpio_dmxtxen.line = gpiod_chip_get_line(gpio_dmxtxen.chip, 18);
			if (0 == gpio_dmxtxen.line) {
				gpiod_chip_close(gpio_dmxtxen.chip);
				gpio_dmxtxen.chip = 0;
				printk(KERN_ERR"!!!!!!! failed to get gpio0.18\n");
			}
			else if (gpiod_line_request_output(gpio_dmxtxen.line, "dmxtxen", 0)) {
				printk (KERN_ERR"!!!!!!! failed to request gpio0.18 as output\n");
				gpiod_line_release(gpio_dmxtxen.line);
				gpiod_chip_close(gpio_dmxtxen.chip);
			}
		}
		else
			printk (KERN_ERR"!!!!!!! failed to create gpio0\n");
	}
}

/*
 * This callback is invoked right before the transmitter starts to
 * send out a transmission buffer.
 */
void test_tx_start (struct rtuart * uart, struct rtuart_buffer * buffer)
{
}

/*
 * This callback is invoked when a transmission buffer has been completely
 * read by the driver.
 */
void test_tx_end1 (struct rtuart * uart, struct rtuart_buffer * buffer)
{
	rtuart_set_notify (uart, RTUART_NOTIFY_TRANSMITTER_EMPTY);
}

/*
 * This callback is invoked when a transmission has physically completed.
 */
void test_tx_end2 (struct rtuart * uart, struct rtuart_buffer * buffer)
{
	rs485txen_set(0);
	dmx_tx_ready = 1;
}

static void test_input_change (struct rtuart * uart,
			       unsigned long eventmask,
			       unsigned long values)
{
	printf ("rtuart_dev_input_change(mask:%08lX  val:%08lX)\n", eventmask, values);
}

static unsigned char dmx_data[2][513];
static int dmx_slot_count[2];
static int current_frame = 0;
static int rx_frame_ready = 0;
static struct rtuart_buffer dmx_rx_buffer;
/*
 * This callback is invoked on a receive error, the errors mask is passed
 * as parameter.
 */
void test_rx_err (struct rtuart * uart, unsigned long errmask)
{
	if (errmask & RTUART_EVENT_BREAK) {
		const int count = dmx_rx_buffer.transfered;
		dmx_rx_buffer.data = dmx_data[current_frame];
		// dmx_rx_buffer.triggermask = 3;
		rtuart_rx_start (uart, &dmx_rx_buffer);

		current_frame = (current_frame + 1) % 2;
		dmx_slot_count[current_frame] = count;
		rx_frame_ready = 1;
	}
#if 0
	if (errmask) {
		if (errmask & RTUART_EVENT_BREAK)         printf (" BREAK");
		if (errmask & RTUART_EVENT_DSR_CHANGE)    printf (" DSR_CHANGE");
		if (errmask & RTUART_EVENT_DCD_CHANGE)    printf (" DCD_CHANGE");
		if (errmask & RTUART_EVENT_RI_CHANGE)     printf (" RI_CHANGE");
		if (errmask & RTUART_EVENT_OVERRUN_ERROR) printf (" OVERRUN_ERROR");
		if (errmask & RTUART_EVENT_FRAME_ERROR)   printf (" FRAME_ERROR");
		if (errmask & RTUART_EVENT_PARITY_ERROR)  printf (" PARITY_ERROR");
		printf ("\n");
	}
#endif
}

/*
 * This callback is invoked when a character is received but the application
 * was not ready to receive it, the character is passed as parameter.
 */
void test_rx_char (struct rtuart * uart, const unsigned short c)
{
	printf ("test_rx_char(c:%04X)\n", c);
	// rtuart_set_notify (uart, RTUART_NOTIFY_RECEIVER_HASDATA|RTUART_NOTIFY_RECEIVER_TIMEOUT);
}

/*
 * This callback is invoked when a receive buffer has completely been filled.
 * The buffer is removed from the list of active buffers. If this function is
 * not defined, a default function is called, that removed the memory
 * in this buffer to prevent memory leaks.
 */
void test_rx_end (struct rtuart * uart, struct rtuart_buffer * buffer)
{
	printf ("test_rx_end(buffer:%p)\n", buffer);
}

/*
 * there have no characters been received for some time.
 * This is called at most once per buffer. The buffer remains
 * the top element in the active buffer list. This must be done
 * explicitly by a call to rtuart_rx_stop.
 */
void test_rx_timeout (struct rtuart * uart, struct rtuart_buffer * buffer)
{
	return;
	printf ("test_rx_timeout(buffer:%p)  %lu\n",
		buffer,
		buffer ? buffer->transfered : -1
		);
}

/*
 * Is called when a triggerpoint has been passed.
 * A trigger point can be used to initiate an end of buffer.
 * The trigger point that completes a buffer can be changed
 * while the buffer is active, e.g. if the one trigger is
 * set to the payload size and a callback adjusts the
 * end-of-buffer trigger (eob_trigger).
 */
void test_rx_trigger(struct rtuart * uart,
		     struct rtuart_buffer * buffer,
		     const unsigned long triggermask
		     )
{
	printf ("test_rx_trigger(buffer:%p, triggermask=%08lX)\n",
		buffer,
		triggermask);
	printf ("  -> %lu\n", buffer->transfered);
	if (triggermask & 8)
	  buffer->validcount = 20;
}


struct rtuart_callbacks test_cb =
{
	.tx_start   = test_tx_start,
	.tx_end1    = test_tx_end1,
	.tx_end2    = test_tx_end2,
	.input_change = test_input_change,
	.rx_err     = test_rx_err,
	.rx_char    = test_rx_char,
	.rx_end     = test_rx_end,
	.rx_timeout = test_rx_timeout,
	.rx_trigger = test_rx_trigger
};

/* call with: chrt -r -p 99 <pid> */
#include <signal.h>

static volatile int g_run = 0;
static void handle_stop(int sig)
{
	g_run = 0;
}

#include <math.h>

static inline int max(int a, int b)
{
  return (a > b) ? a : b;
}

int main (int argc, char ** argv)
{
	const char * default_uart = "pl011"; // "pc16c550";
	const char * default_uio = "/dev/uio0";

	if (argc > 3) {
		if (strcmp(argv[3], "--debug")==0)
			set_loglevel(LOGLEVEL_DEBUG);
		else if (strcmp(argv[3], "--info")==0)
			set_loglevel(LOGLEVEL_INFO);
	}

	rs485txen_init((argc > 1) ? argv[1] : default_uart);

	uart = rtuart_create((argc > 1) ? argv[1] : default_uart,
			     (argc > 2) ? argv[2] : default_uio);
	if (uart == 0) {
		fprintf(stderr, "failed to create uart\n");
		fprintf(stderr, "usage: %s [pc16c550|pl011] <ui0-device>\n", argv[0]);
		return 1;
	}
	rtuart_assign_callbacks(uart, &test_cb, 0);

	signal(SIGHUP, handle_stop);
	signal(SIGINT, handle_stop);
	signal(SIGQUIT, handle_stop);
	g_run = 1;

	rtuart_set_baudrate(uart, 250000); // 
	rtuart_set_format(uart, 8,'n', 2); // "8n2"
	rtuart_set_handshake(uart, RTUART_HANDSHAKE_OFF);
	rs485txen_set(0);

	rtuart_clr_control(uart, RTUART_CONTROL_LOOP); // disable loop

#if 0 // uart test
	rtuart_set_control(uart, RTUART_CONTROL_LOOP);
	struct rtuart_buffer b;
	memset (&b, 0, sizeof(b));
	b.size = 128;
	b.validcount = b.size;
	b.data = malloc(b.size);
	b.triggermask = 15;
	b.trigger[0] = 2;
	b.trigger[1] = 5;
	b.trigger[2] = 8;
	b.trigger[3] = 10;
	rtuart_rx_start (uart, &b);

	struct rtuart_buffer b2;
	memset (&b2, 0, sizeof(b2));
	b2.data = "Hello,World!!4567890";
	b2.size = strlen(b2.data);
	b2.validcount = b2.size;
	b2.triggermask = 15;
	b2.trigger[0] = 2;
	b2.trigger[1] = 5;
	b2.trigger[2] = 8;
	b2.trigger[3] = 12;
	rtuart_tx_start (uart, &b2);

	sleep(5);

	rtuart_set_notify (uart, RTUART_NOTIFY_RECEIVER_EVENT);
	while(g_run)
	{
		sleep(1);
		printf ("waiting\n");
		rs485txen_set(0);
		sleep(1);
		rs485txen_set(1);
	}
	rtuart_clr_control(uart, RTUART_CONTROL_LOOP);
#endif

#if 0 // Transmitter Test
	const double stretchingFactor = 0.2;
	const double speedFactor = 0.15;

	unsigned char dmx_data[515];
	memset (&dmx_data, 0, sizeof(dmx_data));

	int dmxframe = 0;
	while(g_run)
	{
		printf ("frame %d\n", dmxframe);
		++dmxframe;
		int i;
		for (i=0; i < 512; ++i)
			dmx_data[2+1+i] = max(0, (int)(sin(dmxframe*speedFactor + i*stretchingFactor) * 255.01));

		memset (&dmx_tx_buffer, 0, sizeof(dmx_tx_buffer));
		dmx_tx_buffer.data = dmx_data + 2;
		dmx_tx_buffer.size = sizeof(dmx_data) - 2;
		dmx_tx_buffer.validcount = dmx_tx_buffer.size;
		dmx_tx_buffer.triggermask = 0;

		rs485txen_set(1);
		rtuart_set_control(uart, RTUART_CONTROL_BREAK);
		usleep(88);
		rtuart_clr_control(uart, RTUART_CONTROL_BREAK);

		rtuart_tx_start (uart, &dmx_tx_buffer);
		int count = 0;
		while (g_run && !dmx_tx_ready) {
			usleep(1000);
			if (count++ == 1000) {
				rtuart_handle_irq(uart);
				printf("v:%lu t:%lu\n", dmx_tx_buffer.validcount, dmx_tx_buffer.transfered);
			}
		}
		dmx_tx_ready = 0;
		usleep(2000);
	}
#endif


#if 1 // receiver test
	memset (&dmx_rx_buffer, 0, sizeof(dmx_rx_buffer));
	dmx_rx_buffer.size = 513;
	dmx_rx_buffer.validcount = dmx_rx_buffer.size;
	dmx_rx_buffer.data = 0;
	dmx_rx_buffer.triggermask = 0;
	dmx_rx_buffer.trigger[0] = 1;
	dmx_rx_buffer.trigger[1] = 5;

	if(0)
		rtuart_set_notify (uart,
				   RTUART_NOTIFY_RECEIVER_HASDATA |
				   RTUART_NOTIFY_RECEIVER_TIMEOUT);
	rtuart_set_notify (uart, RTUART_NOTIFY_RECEIVER_EVENT);

	while(1)
	{
		while (g_run && (rx_frame_ready == 0))
			usleep(10*1000);
		if (!g_run)
			break;

		rx_frame_ready = 0;
		int i;
		printf ("[%03d]", dmx_slot_count[current_frame]);
		for (i = 0; i < 16; ++i)
			printf (" %02X", dmx_data[current_frame][i]);
		printf ("\n");
	}
#endif


	printf ("Stopping\n");
	rtuart_clr_notify (uart, RTUART_NOTIFY_RECEIVER_EVENT);

	rtuart_cleanup(uart);
	printf ("\n\n\n");

	return 0;
}
