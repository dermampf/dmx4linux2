obj-m += dmx512-core.o

dmx512-core-objs += dmx512.o dmx512framequeue.o

ccflags-y := -I/home/michael/code/Diva/llg-dmx4linux2/src/include

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
