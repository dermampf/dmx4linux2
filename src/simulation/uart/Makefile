
all : t_pc16cx50

CFLAGS+=-I../hardware-model/
LIBS+=-L../hardware-model/ -lhardware_model

t_pc16cx50 : t_pc16cx50.o pc16cx50.o wbuart_16x50.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

test: t_pc16cx50
	LD_LIBRARY_PATH=../hardware-model/ HARDWARE_MODEL_VDC_FILENAME=test.vdc ./t_pc16cx50


wbuart_16x50.o : ../hardware-model/hardware_model.h wbuart_16x50.h
t_pc16cx50.o : wbuart_16x50.h pc16cx50.h pc16cx50_registers.h
pc16cx50.o : pc16cx50.h pc16cx50_registers.h

clean:
	-rm -f *~ *.o *.vdc t_pc16cx50
