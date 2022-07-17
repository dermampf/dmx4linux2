OBJ=obj-x86

CFLAGS+=-I../../kernel_emulation/waitqueue/
CFLAGS+=-I../../kernel_emulation/kfifo
CFLAGS+=-I../uart/

LDLIBS+=-lpthread

CFLAGS+=-I../hardware-model/
LDLIBS+=-L../hardware-model/ -lhardware_model

CFLAGS+=-g -ggdb

all : $(OBJ) $(OBJ)/t_suart $(OBJ)/t_suart_uio $(OBJ)/t_suart_parport

# $(OBJ)/t_streamuart

obj-x86 :
	mkdir -p obj-x86

$(OBJ)/t_streamuart : \
	t_streamuart.o \
	kfifo.o \
	waitqueue.o \
	streamuart.o \
	streamuart_event.o

$(OBJ)/t_suart : \
	$(OBJ)/t_suart.o \
	$(OBJ)/kfifo.o \
	$(OBJ)/waitqueue.o \
	$(OBJ)/streamuart.o \
	$(OBJ)/streamuart_event.o \
	$(OBJ)/wbuart_16x50.o \
	$(OBJ)/pc16cx50.o \
	$(OBJ)/dmx_device.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)


$(OBJ)/t_suart_parport : \
	$(OBJ)/t_suart.o \
	$(OBJ)/kfifo.o \
	$(OBJ)/waitqueue.o \
	$(OBJ)/streamuart.o \
	$(OBJ)/streamuart_event.o \
	$(OBJ)/wbuart_16x50_parport.o \
	$(OBJ)/pc16cx50.o \
	$(OBJ)/dmx_device.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(OBJ)/t_suart_uio : \
	$(OBJ)/t_suart.o \
	$(OBJ)/kfifo.o \
	$(OBJ)/waitqueue.o \
	$(OBJ)/streamuart.o \
	$(OBJ)/streamuart_event.o \
	$(OBJ)/wbuart_16x50_uio.o \
	$(OBJ)/pc16cx50.o \
	$(OBJ)/dmx_device.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)



test : $(OBJ)/t_suart
	LD_LIBRARY_PATH=../hardware-model/ $<

xsim : $(OBJ)/t_suart
	LD_LIBRARY_PATH=../hardware-model/ HARDWARE_MODEL_VDC_FILENAME=test.vcd $<


clean :
	-rm -f *~ *.o
	-rm -f *.vcd
	-rm -f t_streamuart t_suart t_suart_uio
	-rm -f $(OBJ)/*.o
	-rm -f $(OBJ)/t_streamuart $(OBJ)/t_suart $(OBJ)/t_suart_uio






$(OBJ)/%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ)/%.o : ../../kernel_emulation/kfifo/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ)/%.o : ../../kernel_emulation/waitqueue/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ)/%.o : ../uart/%.c
	$(CC) $(CFLAGS) -c -o $@ $<
