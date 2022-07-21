-include /tmp/dmxconfig.mk

C=	js.c pointer.c ps2.c serial.c
ifdef CONFIG_HAVE_GPM
C+=	gpm.c
CFLAGS+=-DHAVE_GPM=1
endif

OBJ=	$(C:%.c=%.o)
EXE=	pointer.a

all:	$(EXE)

$(EXE):	$(OBJ)
	ar cr $@ $^
	ranlib $@

clean:
	$(RM) *.o $(EXE) Makefile.dep core test *~

test:	test.o $(EXE)
	$(CC) -o $@ $^ ../../libs/libdmx.a -lgpm
