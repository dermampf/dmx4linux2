obj-m += dmx512_dummy.o

#dmx512-dummy-objs += dmx512_dummy.o

ccflags-y := -I/home/michael/code/Diva/llg-dmx4linux2/src/include
KBUILD_EXTRA_SYMBOLS := /home/michael/code/Diva/llg-dmx4linux2/src/drivers/dmx512/core/Module.symvers
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
