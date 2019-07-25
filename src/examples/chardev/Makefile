CFLAGS+=-I../../include
LDLIBS+=-lm

TARGETS=t_dmx512-chardev-write \
	t_dmx512-chardev-read \
	t_dmx512-chardev-read-poll \
	t_dmx512-send-test-pattern \
	t_sinus

all :   $(TARGETS)

install:
	mkdir -p $(HOME)/dmxbin/
	cp $(TARGETS) $(HOME)/dmxbin/

clean:
	-rm -f *.o *~ $(TARGETS)
