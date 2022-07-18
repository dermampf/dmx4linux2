CFLAGS+=-I..
CFLAGS+=-g -ggdb
LDFLAGS+=-g -ggdb

all : t_uio32_bus

clean:
	-rm -f *.o *~ t_uio32_bus

t_uio32_bus : t_uio32_bus.o uio32_bus.o rtuart_bus.o
