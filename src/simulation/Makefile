CFLAGS+=-I../kernel_emulation/waitqueue/
CFLAGS+=-I../kernel_emulation/kfifo

CFLAGS+=-Iuart/
CFLAGS+=-Isuart/

LDLIBS+=-lpthread

CFLAGS+=-Ihardware-model/
LDLIBS+=-Lhardware-model/ -lhardware_model

CFLAGS+=-g -ggdb

OBJ=obj-x86


all : $(OBJ) $(OBJ)/t_suart $(OBJ)/t_suart_uio $(OBJ)/t_suart_parport



$(OBJ) :
	mkdir -p $@

SUART_CORE_OBJS= \
	$(OBJ)/kfifo.o \
	$(OBJ)/waitqueue.o \
	$(OBJ)/streamuart.o \
	$(OBJ)/streamuart_event.o \
	$(OBJ)/pc16cx50.o \
	$(OBJ)/dmx_device.o


$(OBJ)/t_suart : \
	$(OBJ)/t_dmx512_suart.o \
	$(OBJ)/dmx512_suart.o \
	$(SUART_CORE_OBJS) \
	$(OBJ)/wbuart_16x50.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)


$(OBJ)/t_suart_parport : \
	$(OBJ)/t_dmx512_suart.o \
	$(OBJ)/dmx512_suart.o \
	$(SUART_CORE_OBJS) \
	$(OBJ)/wbuart_16x50_parport.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(OBJ)/t_suart_uio : \
	$(OBJ)/t_dmx512_suart.o \
	$(OBJ)/dmx512_suart.o \
	$(SUART_CORE_OBJS) \
	$(OBJ)/wbuart_16x50_uio.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)


test : $(OBJ)/t_suart
	LD_LIBRARY_PATH=hardware-model/ $<

xsim : $(OBJ)/t_suart
	LD_LIBRARY_PATH=hardware-model/ HARDWARE_MODEL_VDC_FILENAME=test.vcd $<


clean :
	-rm -f *~ *.o
	-rm -f *.vcd
	-rm -f t_streamuart t_suart t_suart_uio
	-rm -f $(OBJ)/*.o
	-rm -f $(OBJ)/t_streamuart $(OBJ)/t_suart $(OBJ)/t_suart_uio


$(OBJ)/%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ)/%.o : ../kernel_emulation/kfifo/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ)/%.o : ../kernel_emulation/waitqueue/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ)/%.o : uart/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ)/%.o : suart/%.c
	$(CC) $(CFLAGS) -c -o $@ $<