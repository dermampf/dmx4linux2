CFLAGS+=-I../../include
LDLIBS+=-lm

TARGETS=$(OBJDIR)t_dmx512-chardev-write \
	$(OBJDIR)t_dmx512-chardev-write-one \
	$(OBJDIR)t_dmx512-chardev-write-multiport \
	$(OBJDIR)t_dmx512-chardev-read \
	$(OBJDIR)t_dmx512-chardev-read-one \
	$(OBJDIR)t_dmx512-chardev-read-with-filter \
	$(OBJDIR)t_dmx512-chardev-read-poll \
	$(OBJDIR)t_dmx512-send-test-pattern \
	$(OBJDIR)t_dmx512_recv_test_pattern \
	$(OBJDIR)t_dmx512-chardev-select \
	$(OBJDIR)t_dmx512-chardev-set-filter-and-loop-get-filter \
	$(OBJDIR)t_dmx512-chardev-toomanyopen \
	$(OBJDIR)t_dmx512-chardev-info \
	$(OBJDIR)t_sinus

all ::   $(TARGETS)

install:
	mkdir -p $(HOME)/dmxbin/
	cp $(TARGETS) $(HOME)/dmxbin/

clean:
	-rm -f *.o *~ $(TARGETS)

$(OBJDIR)% : $(OBJDIR)%.o

$(OBJDIR)%.o : $(OBJDIR)%.c
