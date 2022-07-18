#include <rtuart_factory.h>

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <rtuart.h>
#include <rtuart_priv.h>
#include <rtuart_pc16c550.h>
#include <rtuart_pl011.h>
#include <uio32_bus.h>
#include <dummy_bus.h>

#include <kernel.h>

static struct rtuart * g_uart = 0;
static int g_run_irq = 0;
static int g_uio_fd = -1;
//static posix_semaphore txempty_semaphore;
static pthread_t  irq_handler;

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

static void * irq_handler_function (void * arg)
{
	printf("irq_handler started\n");
	uio_set_interrupt_enable(g_uio_fd, 1);
	while (g_run_irq)
	{
		struct pollfd fds[1];
                memset (fds, 0, sizeof (fds));
                fds[0].fd = g_uio_fd;
                fds[0].events = POLLIN;
		int ret = poll(fds, sizeof(fds)/sizeof(fds[0]), 1); /* later reduce to 1 */
		if (ret < 0) {
			perror("wait for irq");
			sleep(1);
			continue;
		}
		else if (ret == 0)
                {
		        // timeout
                }
		else if (fds[0].revents == POLLIN)
		{
                        if (uio_handle_irq(fds[0].fd))
				rtuart_handle_irq(g_uart);
			uio_set_interrupt_enable(g_uio_fd, 1);
		}
		run_all_tasklets();
	}
	uio_set_interrupt_enable(g_uio_fd, 0);
	printf("#### irq_handler exited\n");
	fflush(stdout);
	return 0;
}

struct rtuart * rtuart_create(const char * name, const char * bus_name)
{
        struct rtuart * uart = 0;
        struct rtuart_bus * bus = 0;

	if (strcmp(bus_name, "dummy")==0)
		bus = dummy_bus_create("");
	else if (strncmp(bus_name, "dummy:", 6)==0)
		bus = dummy_bus_create(&bus_name[6]);
	else
	{
		const char * uio_device = bus_name ? bus_name : "/dev/uio0";
		if ((g_uio_fd = open(uio_device, O_RDWR)) == -1)
		{
			perror("open uio");
			return 0;
		}

		if (strcmp(name, "pc16c550") == 0)
			bus = uio32_create(g_uio_fd, /*offset:*/0x1000, /*size:*/0x10000);
		else if (strcmp(name, "pl011") == 0)
			bus = uio32_create(g_uio_fd, /*offset:*/0, /*size:*/0x1000);

		g_run_irq = 1;
	}

	if (bus) {
		if (strcmp(name, "pc16c550") == 0)
			uart = rtuart_pc16c550_create(bus, 100*1000*1000);

		else if (strcmp(name, "pl011") == 0) {
			uart = rtuart_pl011_create(bus, 48*1000*1000);
		}

		if (uart) {
			g_uart = uart;
			if (g_run_irq)
				if (pthread_create(&irq_handler, NULL, irq_handler_function, 0/*&queue*/))
					printf ("failed to create thread\n");
			return uart;
		}

		free(bus);
	}
        return 0;
}

void rtuart_cleanup(struct rtuart * uart)
{
	if (uart != g_uart)
		return;

	if (g_run_irq) {
		printf ("stopping irq thread\n");
		g_run_irq = 0;
		pthread_join(irq_handler, NULL);
	}
	g_uart = 0;

	if (uart->bus)
		free(uart->bus);
	if (uart)
		free(uart);
}
