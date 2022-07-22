#include "uio.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

struct uart
{
    struct uio_handle * uio;
    pthread_t  irq_thread;
    int        run;
};

static void uart_interrupt (struct uart *u);

static void * interrupt_thread_function(void * arg)
{
    struct uart * u = (struct uart *)arg;
    while (1)
    {
	struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 500*1000;

	if (!u->run)
	    return 0;

	uio_enable_interrupt(u->uio);
	if (uio_wait_for_interrupt_timeout(u->uio, &timeout) >= 0)
	    uart_interrupt (u);
    }
}


#include "../simulation/uart/pc16cx50_registers.h"

static void uart_interrupt (struct uart *u)
{
    const int ier = uio_peek8(u->uio, PC16550_REG_IIR);
    printf ("IER:%02X\n", ier);
}

int main (int argc, char **argv)
{
    const char * uio_name = (argc > 1) ? argv[1] : 0;
    if (uio_name)
    {
	struct uart uart;
	uart.uio = uio_open (uio_name, 4096);
	if (!uart.uio)
	    return 1;

	// set Baudrate
	uio_poke8(uart.uio, PC16550_REG_LCR, PC16550_LCR_DLAB | PC16550_LCR_WLS0|PC16550_LCR_WLS1);
	uio_poke8(uart.uio, PC16550_REG_DLL, 1);
	uio_poke8(uart.uio, PC16550_REG_DLM, 0x80);
	uio_poke8(uart.uio, PC16550_REG_LCR, PC16550_LCR_WLS0|PC16550_LCR_WLS1);


	uart.run = 1;
	pthread_create(&uart.irq_thread, NULL, interrupt_thread_function, &uart);
	uio_poke8(uart.uio, PC16550_REG_IER, PC16C550_IER_TRANSMITTER_HOLDING_REGISTER_EMPTY);

	sleep(1);

	int i;
	for (i = 0; i < 16; ++i)
	{
	    printf ("write THR\n");
	    uio_poke8(uart.uio, PC16550_REG_THR, i);
	    sleep(1);
	}

	uio_poke8(uart.uio, PC16550_REG_IER, 0);

	printf ("stop irq handler\n");
	uio_disable_interrupt(uart.uio);
	uart.run = 0;
	printf ("waiting for irq-handler to stop\n");
	pthread_join(uart.irq_thread, 0);
	printf ("irq-handler stopped\n");
	uio_close(uart.uio);
	return 0;
    }
    printf ("usage: %s <uio-name>\n", argv[0]);
    return 0;
}
