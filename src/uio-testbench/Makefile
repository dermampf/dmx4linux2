CFLAGS+=-Ibus -Iuart -Idmx512 -I. -I../include/
CFLAGS+=-g -ggdb
LFLAGS+=-g -ggdb
LDFLAGS+=-g -ggdb
LDLIBS+=-lpthread -lm -lrt

all::  $(OBJDIR)t_pc16c550 $(OBJDIR)t_pl011

clean:
	-rm -f *~ */*~ $(OBJDIR)*.o $(OBJDIR)t_pc16c550 $(OBJDIR)t_pl011

$(OBJDIR)t_pc16c550 : $(OBJDIR)t_pc16c550.o $(OBJDIR)pc16c550.o $(OBJDIR)uio32_bus.o $(OBJDIR)rtuart_bus.o $(OBJDIR)rtuart_dmx512_client.o

$(OBJDIR)t_pl011 : $(OBJDIR)t_pl011.o $(OBJDIR)pl011.o $(OBJDIR)uio32_bus.o $(OBJDIR)rtuart_bus.o $(OBJDIR)rtuart_dmx512_client.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -lgpiod -o $@

$(OBJDIR)%.o : bus/%.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(OBJDIR)%.o : uart/%.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(OBJDIR)%.o : dmx512/%.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(OBJDIR)%.o : %.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<
