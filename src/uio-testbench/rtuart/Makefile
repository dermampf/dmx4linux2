CFLAGS+=-I.
CFLAGS+=-I../bus/
CFLAGS+=-I../
CFLAGS+=-I../../include/
CFLAGS+=-g -ggdb
LDFLAGS+=-g -ggdb
LDLIBS+=-lpthread
LDLIBS+=-lm
LDLIBS+=-lgpiod
LDLIBS+=-lrt
#CFLAGS+=-Wall

all :: $(OBJDIR)t_rtuart $(OBJDIR)t_dmxrtuart

$(OBJDIR)t_rtuart : $(OBJDIR)t_rtuart.o $(OBJDIR)rtuart.o $(OBJDIR)rtuart_pc16c550.o $(OBJDIR)rtuart_pl011.o $(OBJDIR)rtuart_bus.o $(OBJDIR)uio32_bus.o $(OBJDIR)dummy_bus.o $(OBJDIR)rtuart_factory.o $(OBJDIR)kernel.o

$(OBJDIR)t_dmxrtuart : $(OBJDIR)t_dmxrtuart.o $(OBJDIR)rtuart.o $(OBJDIR)rtuart_pc16c550.o $(OBJDIR)rtuart_pl011.o $(OBJDIR)rtuart_bus.o $(OBJDIR)uio32_bus.o $(OBJDIR)dummy_bus.o $(OBJDIR)rtuart_factory.o $(OBJDIR)kernel.o

clean ::
	-rm -f *~ $(OBJDIR)*.o

distclean: clean
	-rm -f $(OBJDIR)t_rtuart $(OBJDIR)t_dmxrtuart

$(OBJDIR)%.o: ../bus/%.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(OBJDIR)kernel.o: ../kernel.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<
