#include <pl011.h>
#include <pc16c550.h>
#include <rtuart.h>
#include <rtuart_client.h>
#include <uio32_bus.h>
#include <kernel.h>

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <poll.h>
#include <time.h>

extern void pc16c550_periodic_check(struct rtuart * u);
extern void pl011_periodic_check(struct rtuart * u);
static void (*uart_periodic_check)(struct rtuart * u) = 0;
static struct rtuart * rtuart_create(const char * name, const int busfd)
{
	struct rtuart * uart = 0;
	struct rtuart_bus * bus = 0;

	if (strcmp(name, "pc16c550") == 0) {
		uart = pc16c550_create(100*1000*1000);
		bus = uio32_create(busfd, /*offset:*/0x1000, /*size:*/0x10000);
		uart_periodic_check = pc16c550_periodic_check;
	}
	else if (strcmp(name, "pl011") == 0) {
		uart = pl011_create(48*1000*1000);
		bus = uio32_create(busfd, /*offset:*/0, /*size:*/0x1000);
		uart_periodic_check = pl011_periodic_check;
	}
	if (bus && uart) {
		uart->bus = bus;
		return uart;
	}
	if (bus)
		free(bus);
	if (uart)
		free(uart);
	return 0;
}

static int test_transmitter_has_space(struct rtuart * uart, const int space_available)
{
	printf ("test_transmitter_has_space\n");
	uart_enable_notification(uart , UART_NOTIFY_TXEMPTY);
	return 0;
}

static int test_transmitter_empty(struct rtuart * uart)
{
	printf ("test_transmitter_empty\n");
	uart_enable_notification(uart , UART_NOTIFY_TXREADY);
	return 0;
}

static int test_receiver_data_available(struct rtuart * uart, const int count)
{
	printf ("test_receiver_data_available\n");
	uart_enable_notification(uart , UART_NOTIFY_RXAVAILABLE);
	return 0;
}

static int test_modem_input_changed(struct rtuart * uart, const unsigned long values, const unsigned long changemask)
{
	printf ("test_modem_input_changed\n");
	uart_enable_notification(uart , UART_NOTIFY_INPUTCHANGED);
	return 0;
}

static int test_line_status_event(struct rtuart * uart, const unsigned long eventmask)
{
	printf ("test_line_status_event\n");
	uart_enable_notification(uart , UART_NOTIFY_LINEEVENT);
	return 0;
}

static const struct rtuart_client_callbacks test_callbacks =
{
	.transmitter_has_space   = test_transmitter_has_space,
	.transmitter_empty       = test_transmitter_empty,
	.receiver_data_available = test_receiver_data_available,
	.modem_input_changed     = test_modem_input_changed,
	.line_status_event       = test_line_status_event
};


static int uio_handle_irq(int fd)
{
	u32 info;
	const int nb = read(fd, &info, sizeof(info));
	if (nb == (ssize_t)sizeof(info))
	{
		return 1;
		printf("Interrupt #%lu!\n", info);
	}
	return 0;
}

static void uio_set_interrupt_enable(const int fd, const int on)
{
        u32 info = on; /* 1=unmask, 0=mask */
        ssize_t nb = write(fd, &info, sizeof(info));
        if (nb != (ssize_t)sizeof(info)) {
		perror("write");
		close(fd);
		exit(EXIT_FAILURE);
        }
}


int main (int argc, char **argv)
{
	struct rtuart_client client;
	const int busfd = open((argc > 2) ? argv[2] : "/dev/uio0", O_RDWR);
	if (busfd == -1)
	{
		perror("open uio");
		return 0;
	}

	struct rtuart * uart = rtuart_create(argv[1], busfd);
	if (!uart)
		return 1;

	uart->client = &client;
	client.callbacks = &test_callbacks;

	struct timespec last;
	bzero(&last, sizeof(last));
	clock_gettime(CLOCK_MONOTONIC, &last);

	rtuart_set_baudrate(uart, 250000);
	rtuart_set_break(uart, 0);
	rtuart_set_format(uart, 8, 'n', 2);
	uart_enable_notification(uart , UART_NOTIFY_TXEMPTY | UART_NOTIFY_TXREADY | UART_NOTIFY_RXAVAILABLE | UART_NOTIFY_INPUTCHANGED | UART_NOTIFY_LINEEVENT);

	uio_set_interrupt_enable(busfd, 1);
	while (1)
	{
		struct pollfd fds[2];
		memset (fds, 0, sizeof (fds));
		fds[0].fd = busfd;
		fds[0].events = POLLIN;
		fds[1].fd = 0;
		fds[1].events = POLLIN;
		int ret = poll(fds, sizeof(fds)/sizeof(fds[0]), 0);

		if (ret < 0)
			return ret;
		else if (ret == 0)
		{
			if (uart_periodic_check)
				uart_periodic_check(uart);
			continue;
		}

		if (fds[0].revents == POLLIN)
		{
			if (uio_handle_irq(fds[0].fd) && uart->ops->handle_irq)
			{
				rtuart_handle_irq(uart);
				uio_set_interrupt_enable(busfd, 1);
			}
		}
		
		if (fds[1].revents == POLLIN)
		{
			char buffer[200];
			if (fgets(buffer, sizeof(buffer), stdin))
			{
				rtuart_write_chars (uart, buffer, strlen(buffer));
			}
		}
	  }
	uio_set_interrupt_enable(busfd, 0);

	return 0;
}
