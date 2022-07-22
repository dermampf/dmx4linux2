CFLAGS+=-I../kernel_emulation/waitqueue/
CFLAGS+=-I../kernel_emulation/kfifo
CFLAGS+=-I../kernel_emulation/include/

CFLAGS+=-Iuart/
CFLAGS+=-Isuart/

LDLIBS+=-lpthread

SIM_CFLAGS+=-Ihardware-model/
SIM_LDLIBS+=-Lhardware-model/ -lhardware_model

CFLAGS+=-I../include/
CFLAGS+=-I../cuse-drivers/

CFLAGS+=-g -ggdb

ARCH?=x86
OBJ=obj-$(ARCH)


all :   $(OBJ) \
	$(OBJ)/t_suart \
	$(OBJ)/t_suart_uio \
	$(OBJ)/t_suart_parport \
	$(OBJ)/dmx512_cuse_suart \
	$(OBJ)/dmx512_cuse_suart_parport \
	$(OBJ)/dmx512_cuse_suart_uio



$(OBJ) :
	mkdir -p $@

SUART_CORE_OBJS= \
	$(OBJ)/kfifo.o \
	$(OBJ)/waitqueue.o \
	$(OBJ)/streamuart.o \
	$(OBJ)/streamuart_event.o \
	$(OBJ)/pc16cx50.o



$(OBJ)/dmx512_cuse_suart : \
	$(OBJ)/dmx512_cuse_suart.o \
	$(OBJ)/dmx512_cuse_dev.o \
	$(OBJ)/dmx512framequeue.o \
	$(OBJ)/dmx512_suart.o \
	$(SUART_CORE_OBJS) \
	$(OBJ)/wbuart_16x50.o $(OBJ)/dmx_device.o
	$(CC) $(CFLAGS) $(SIM_CFLAGS) -o $@ $^ $(LDLIBS) $(SIM_LDLIBS) -lfuse

$(OBJ)/dmx512_cuse_suart_uio : \
	$(OBJ)/dmx512_cuse_suart.o \
	$(OBJ)/dmx512_cuse_dev.o \
	$(OBJ)/dmx512framequeue.o \
	$(OBJ)/dmx512_suart.o \
	$(SUART_CORE_OBJS) \
	$(OBJ)/wbuart_16x50_uio.o $(OBJ)/uio.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS) -lfuse

$(OBJ)/dmx512_cuse_suart_parport : \
	$(OBJ)/dmx512_cuse_suart.o \
	$(OBJ)/dmx512_cuse_dev.o \
	$(OBJ)/dmx512framequeue.o \
	$(OBJ)/dmx512_suart.o \
	$(SUART_CORE_OBJS) \
	$(OBJ)/wbuart_16x50_parport.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS) -lfuse

$(OBJ)/t_suart : \
	$(OBJ)/t_dmx512_suart.o \
	$(OBJ)/dmx512_suart.o \
	$(SUART_CORE_OBJS) \
	$(OBJ)/wbuart_16x50.o $(OBJ)/dmx_device.o
	$(CC) $(CFLAGS) $(SIM_CFLAGS) -o $@ $^ $(LDLIBS) $(SIM_LDLIBS)


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
	$(OBJ)/wbuart_16x50_uio.o $(OBJ)/uio.o
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

$(OBJ)/%.o : ../uio/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ)/%.o : ../cuse-drivers/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ)/%.o : ../drivers/dmx512/core/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ)/wbuart_16x50.o : uart/wbuart_16x50.c
	$(CC) $(CFLAGS) $(SIM_CFLAGS) -c -o $@ $<

