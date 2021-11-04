OBJDIR=obj/

CFLAGS+=-I../include/
LDLIBS+=-lpthread
CFLAGS+=-g -ggdb

TARGETS=$(OBJDIR)dmx512_cuse_loop

all :: $(OBJDIR) $(TARGETS)

$(OBJDIR)t_rtuart_userspace : $(OBJDIR)t_rtuart_userspace.o


$(OBJDIR)dmx512_cuse_loop : $(OBJDIR)dmx512_cuse_loop_device.o $(OBJDIR)dmx512_cuse_dev.o
	$(CC) -o $@ $^ $(LDLIBS) -lfuse

clean ::
	-rm -f *~ $(OBJDIR)*.o

distclean :: clean
	-rm -f *~ $(OBJDIR)*.o  $(TARGETS)
	-rmdir -p $(OBJDIR)

$(OBJDIR):
	mkdir -p $@

$(OBJDIR)%.o: %.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<
