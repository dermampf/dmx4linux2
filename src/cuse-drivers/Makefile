OBJDIR=obj/
TARGETHOST=root@192.168.10.66
CFLAGS+=-I../include/ -I../kernel_emulation/include/
LDLIBS+=-lpthread
CFLAGS+=-g -ggdb
CFLAGS+=-D_FILE_OFFSET_BITS=64
CFLAGS+=-DFUSE_USE_VERSION=30

TARGETS=$(OBJDIR)dmx512_cuse_loop $(OBJDIR)dmx512_cuse_uart $(OBJDIR)dmx512_cuse_pwm $(OBJDIR)dmx512_cuse_llgip

all :: $(OBJDIR) $(TARGETS)

DMX512CORE_OBJS=$(OBJDIR)dmx512framequeue.o $(OBJDIR)dmx512_cuse_dev.o


$(OBJDIR)t_rtuart_userspace : $(OBJDIR)t_rtuart_userspace.o


$(OBJDIR)dmx512_cuse_loop : $(OBJDIR)dmx512_cuse_loop_device.o $(DMX512CORE_OBJS)
	$(CC) -o $@ $^ $(LDLIBS) -lfuse

$(OBJDIR)dmx512_cuse_uart : $(OBJDIR)dmx512_cuse_uart.o $(DMX512CORE_OBJS)
	$(CC) -o $@ $^ $(LDLIBS) -lfuse

$(OBJDIR)dmx512_cuse_pwm : $(OBJDIR)dmx512_cuse_pwm.o $(OBJDIR)uio.o $(OBJDIR)parse_dmx_uio_args.o $(DMX512CORE_OBJS)
	$(CC) -o $@ $^ $(LDLIBS) -lfuse

$(OBJDIR)dmx512_cuse_llgip : $(OBJDIR)dmx512_cuse_llgip.o $(OBJDIR)uio.o $(DMX512CORE_OBJS)
	$(CC) -o $@ $^ $(LDLIBS) -lfuse

$(OBJDIR)t_uiospeed : $(OBJDIR)t_uiospeed.o $(OBJDIR)uio.o
	$(CC) -o $@ $^ $(LDLIBS)


clean ::
	-rm -f *~ $(OBJDIR)*.o

distclean :: clean
	-rm -f *~ $(OBJDIR)*.o  $(TARGETS)
	-rmdir -p $(OBJDIR)

$(OBJDIR):
	mkdir -p $@

$(OBJDIR)%.o: %.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(OBJDIR)%.o: ../uio/%.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(OBJDIR)%.o: ../drivers/dmx512/core/%.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<


copy: $(TARGETS)
	ssh $(TARGETHOST) mkdir -p cuse_drivers
	scp $(TARGETS) $(TARGETHOST):cuse_drivers/
