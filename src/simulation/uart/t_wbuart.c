#include "wbuart_16x50.h"
#include "pc16cx50.h"
#include <stdio.h>
#include <time.h>
#include <unistd.h>

void irqfunc(int i, void * a)
{
    printf ("irq\n");
}

static void pc16x50_reg_write(struct pc16x50 * uart, int reg, u8 value); //{ uart->write_reg(uart->reg_context, reg, value); }
static u8 pc16x50_reg_read(struct pc16x50 * uart, int reg); // { return uart->read_reg(uart->reg_context, reg); }


int main ()
{
    struct pc16x50 * u = wbuart_pc16x50_create();
    wbuart_set_irqhandler(u, irqfunc, 0);

    while (1)
    {
	pc16x50_reg_write(u, 0xaa, 0x55);
	sleep(1);
    }

    wbuart_delete(u);
    return 0;
}

#include "wbuart_16x50_parport.c"
#include "pc16cx50.c"
